#version 460

#extension GL_ARB_compute_variable_group_size : enable

precision highp float;
precision highp int;

layout (local_size_variable) in;

struct Vector
{
    float x, y, z;
};

struct Boid
{
    Vector pos, vel;
    Vector triangles[6*4];
};

layout (std430, binding = 0) buffer boidsDataBuffer
{
    Boid boidsData[];
};

layout (std430, binding = 1) readonly buffer rayDirsBuffer
{
    Vector rayDirs[];
};

struct Cube
{
    int edgeNodes[12];
    int configuration;
};

layout (std430, binding = 2) readonly buffer cubesBuffer
{
    Cube cubes[];
};


uniform float deltaTime;
uniform vec3 boundingBox;
uniform float predictionLength;
uniform float minSpeed;
uniform float maxSpeed;
uniform float maxForce;
uniform float viewRadius;
uniform float viewAngle;
uniform bool avoidMesh;
uniform float cubeSize;
uniform ivec3 cubeGridDims;
uniform float avoidRadius;
uniform float cohesionCoef;
uniform float alignmentCoef;
uniform float separationCoef;
uniform float obstacleCoef;
uniform vec3 boidModelTriangles[6*4];
uniform int numBoids;

float rayMarchStepSize;
int maxRayMarchSteps;
vec3 boxMaxCorner;
vec3 boxMinCorner;


// vector utility functions

vec3 getVector(Vector v) {
    return vec3(v.x, v.y, v.z);
}

Vector getVector(vec3 v) {
    return Vector(v.x, v.y, v.z);
}

float angleBetween(vec3 v1, vec3 v2){
    return acos(dot(normalize(v1), normalize(v2)));
}

// 3D rotation matrix of an angle around an axis
mat3 rot(vec3 u, float a){
    float x = u.x;
    float y = u.y;
    float z = u.z;
    float c = cos(a);
    float s = sin(a);
    float t = 1.-c;
    return mat3(
    	c+x*x*t, x*y*t-z*s, x*z*t+y*s,
        y*x*t+z*s, c+y*y*t, y*z*t-x*s,
        z*x*t-y*s, z*y*t+x*s, c+z*z*t
    );
}

// calculates the world to local transformation (rotation) matrix for a direction vector
// dir is supposed to be normalized
mat3 transformDirection(vec3 dir, vec3 ref){
    vec3 axis = normalize(-cross(dir, ref));
    float angle = acos(dot(dir, ref));
    return rot(axis, angle);
}

// test intersection of a ray with the AABB bounding box, returns the distance to the intersection points
// from : https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
/*vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir) {
    vec3 tMin = (boxMinCorner - rayOrigin) / rayDir;
    vec3 tMax = (boxMaxCorner - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
};*/

// function adapted from : https://stackoverflow.com/questions/12751080/glsl-point-inside-box-test/37426532
// returns 1 if p is inside the box, 0 otherwise
float insideBox(vec3 p) {
    vec3 s = step(boxMinCorner, p) - step(boxMaxCorner, p);
    return s.x * s.y * s.z;
}

int indexCube(int x, int y, int z){
    return z + cubeGridDims.z * y + cubeGridDims.z * cubeGridDims.y * x;
}

Cube cubeAtPos(vec3 pos){
    pos += boundingBox/2.f;
    pos /= cubeSize;
    ivec3 coords = ivec3(floor(pos));
    int index = indexCube(coords.x, coords.y, coords.z);
    return cubes[index];
}

// test if a ray interseect the mesh (marching cubes)
// returns 1 if intersect, 0 otherwise
float intersectMesh(vec3 pos, vec3 dir){
    vec3 p = pos;
    for(int j = 0; j < maxRayMarchSteps; ++j){
        p += dir * rayMarchStepSize;

        if(insideBox(p) < 1.)
            break;

        if(cubeAtPos(p).configuration != 0)
            return 1.; // distance(p, pos)
    }

    return 0.;
}

// return the closest unobstructed direction to the specified forward direction starting from pos
vec3 findUnobstructedDir(vec3 pos, vec3 forward, out bool allDirsObstructed, out bool isHeadingCollision)
{
    allDirsObstructed = false;

    isHeadingCollision = insideBox(pos + forward * predictionLength) < 1.;
    if(avoidMesh)
        isHeadingCollision = isHeadingCollision || intersectMesh(pos, forward) > 0.;

    if(!isHeadingCollision)
        return vec3(0);

    // get the world to local rotation matrix to rotate the directions
    // in order to check directions close to the boid orientation first
    mat3 transform = transformDirection(forward, vec3(0, 0, 1));

    for(int i = 0; i < rayDirs.length(); ++i){
        vec3 dir = getVector(rayDirs[i]) * transform;
        bool hit = insideBox(pos + dir * predictionLength) < 1.;
        if(!hit && avoidMesh)
            hit = hit || intersectMesh(pos, dir) > 0.;

        if(!hit)
            return dir;
    }

    allDirsObstructed = true;

    return forward;
}

// returns the force vector to steer towards the desired velocity
vec3 steeringForce(vec3 vel, vec3 desired){
    vec3 force = normalize(desired) * maxSpeed - vel;
    float mag = length(force);
    if(isnan(mag) || isinf(mag))
        return vec3(0);
    return normalize(force) * min(mag, maxForce);
}

void main() {
    int id = int(gl_GlobalInvocationID.x);

    Boid thisBoid = boidsData[id];
    vec3 pos = getVector(thisBoid.pos);
    vec3 vel = getVector(thisBoid.vel);
    vec3 dir = normalize(vel);

    // constants
    rayMarchStepSize = cubeSize / 2.;
    maxRayMarchSteps = int(predictionLength / rayMarchStepSize);
    boxMaxCorner = boundingBox / 2.;
    boxMinCorner = -boundingBox / 2.;

    // collision detection
    bool allDirsObstructed, isHeadingCollision;
    vec3 unobstructedDir = findUnobstructedDir(pos, dir, allDirsObstructed, isHeadingCollision);

    // reset position if all paths are blocked (= is inside the mesh) or if the boid
    // got out of the box
    if(/*allDirsObstructed || */insideBox(pos) < 1.)
        pos = vec3(0);

    // apply the normals rules of boid flocking

    float numFlockMates = 0.;
    vec3 flockHeading = vec3(0);
    vec3 flockCenter = vec3(0);
    vec3 separationHeading = vec3(0);

    for(int i = 0; i < numBoids; ++i){
        if(i != id){
            Boid other = boidsData[i];
            vec3 otherPos = getVector(other.pos);
            vec3 otherVel = getVector(other.vel);

            vec3 offset = otherPos - pos;
            float dst = length(offset);

            if(dst < viewRadius){
                if(angleBetween(vel, offset) < viewAngle){
                    numFlockMates += 1.;
                    flockHeading += otherVel;
                    flockCenter += otherPos;
                }

                if(dst < avoidRadius){
                    separationHeading -= offset / dst;
                }
            }
        }
    }

    vec3 acc = vec3(0);

    if(numFlockMates > 0.){
        flockCenter /= numFlockMates;
        vec3 offsetToFlockCenter = flockCenter - pos;

        acc += steeringForce(vel, flockHeading) * alignmentCoef;
        acc += steeringForce(vel, offsetToFlockCenter) * cohesionCoef;
        acc += steeringForce(vel, separationHeading) * separationCoef;
    }

    if(isHeadingCollision){
        acc += steeringForce(vel, unobstructedDir) * obstacleCoef;
    }

    vel += acc * deltaTime;
    float speed = length(vel);
    dir = vel / speed;
    speed = clamp(speed, minSpeed, maxSpeed);
    vel = dir * speed;
    pos += vel * deltaTime;

    // rotate the model shape to match the current boid's direction
    // and calculate triangles normals

    mat3 transform = transformDirection(dir, vec3(0, 1, 0));

    Vector triangles[6*4];

    for(int i = 0; i < 6*4; i += 4){
        vec3 v1 = boidModelTriangles[i] * transform + pos;
        vec3 v2 = boidModelTriangles[i+1] * transform + pos;
        vec3 v3 = boidModelTriangles[i+2] * transform + pos;
        vec3 nor = normalize(cross(v2-v1, v3-v1));

        triangles[i]   = getVector(v1);
        triangles[i+1] = getVector(v2);
        triangles[i+2] = getVector(v3);
        triangles[i+3] = getVector(nor);
    }

    // apply modifications in the copy boid buffer part
    boidsData[id + numBoids] = Boid(getVector(pos), getVector(vel), triangles);
}
