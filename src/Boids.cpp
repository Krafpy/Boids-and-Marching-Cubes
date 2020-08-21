#include "Boids.h"

#include <math.h>

#define BOIDS_SSB_BP    0
#define RAYS_SSB_BP     1
#define CUBES_SSB_BP    2

#define PI  3.14159215
#define PHI 1.61803398


Boids::Boids()
{

}

Boids::Boids(int _numBoids, float width, float height, int _numRays) : numBoids(_numBoids), boidWidth(width), boidHeight(height), numRays(_numRays)
{
    calculateBoidShape();
    setup();
}

bool Boids::setup(int _numBoids, float width, float height, int _numRays)
{
    numBoidsChanged = numBoidsChanged || _numBoids != numBoids;
    numRayDirsChanged = numRayDirsChanged || _numRays != numRays;

    bool result = numBoidsChanged;

    numBoids = _numBoids;
    numRays = _numRays;
    setBoidSize(width, height);
    setup();

    // returns true if the number of boids has changed
    return result;
}

void Boids::setup()
{
    if(!hasProgram)
        createProgram();
    else if(numBoidsChanged)
        updateDispatchParams();

    if(hasBuffers && numBoidsChanged)
        resizeBoidBuffer();
    else if(numBoidsChanged)
        createBuffers();

    if(hasBuffers && numRayDirsChanged)
        calculateRayDirs();

    numBoidsChanged = false;
    numRayDirsChanged = false;
}

void Boids::generateBoids()
{
    // generates a numBoids boids center at 0 with a random velocity
    Boid *boids = new Boid[numBoids * 2];
    for(int i = 0; i < numBoids; i++){
        boids[i] = Boid(vec3d(), vec3d::unitRandom() * maxSpeed);
        boids[numBoids + i] = boids[i];
    }
    boidsData.setSubData(0, boidsData.size, boids);
    delete[] boids;

    // create the color for each boid
    if(!colorsArrayExist){
        colorsArrayExist = true;
    } else {
        delete[] colors;
    }
    colors = new Color[numBoids];
    for(int i = 0; i < numBoids; i++){
        // add some randomness in boids colors
        float roff = ((float)rand()/(float)RAND_MAX - 0.5f) * colorDeviation;
        float goff = ((float)rand()/(float)RAND_MAX - 0.5f) * colorDeviation;
        float boff = ((float)rand()/(float)RAND_MAX - 0.5f) * colorDeviation;
        colors[i].r = mainColor.r + roff;
        colors[i].g = mainColor.g + goff;
        colors[i].b = mainColor.b + boff;
    }
}

void Boids::update(float deltaTime)
{
    boidsData.setBindingPoint(BOIDS_SSB_BP);
    rayDirs.setBindingPoint(RAYS_SSB_BP);
    cubes->setBindingPoint(CUBES_SSB_BP);

    useProgram(boidProgram);
    glUniform1f(glGetUniformLocation(currentProgram.id, "deltaTime"), deltaTime);
    glUniform3f(glGetUniformLocation(currentProgram.id, "boundingBox"), box.x, box.y, box.z);
    glUniform1f(glGetUniformLocation(currentProgram.id, "predictionLength"), predictionLength);
    glUniform1f(glGetUniformLocation(currentProgram.id, "minSpeed"), minSpeed);
    glUniform1f(glGetUniformLocation(currentProgram.id, "maxSpeed"), maxSpeed);
    glUniform1f(glGetUniformLocation(currentProgram.id, "maxForce"), maxForce);
    glUniform1f(glGetUniformLocation(currentProgram.id, "viewAngle"), viewAngle);
    glUniform1f(glGetUniformLocation(currentProgram.id, "viewRadius"), viewRadius);
    glUniform1i(glGetUniformLocation(currentProgram.id, "avoidMesh"), avoidMesh);
    glUniform1f(glGetUniformLocation(currentProgram.id, "cubeSize"), cubeSize);
    glUniform3i(glGetUniformLocation(currentProgram.id, "cubeGridDims"), cubeGrid.x, cubeGrid.y, cubeGrid.z);
    glUniform1f(glGetUniformLocation(currentProgram.id, "avoidRadius"), avoidRadius);
    glUniform1f(glGetUniformLocation(currentProgram.id, "cohesionCoef"), cohesionCoef);
    glUniform1f(glGetUniformLocation(currentProgram.id, "alignmentCoef"), alignmentCoef);
    glUniform1f(glGetUniformLocation(currentProgram.id, "separationCoef"), separationCoef);
    glUniform1f(glGetUniformLocation(currentProgram.id, "obstacleCoef"), obstacleCoef);
    glUniform3fv(glGetUniformLocation(currentProgram.id, "boidModelTriangles"), 6*4, (float*)boidModelTriangles);
    glUniform1i(glGetUniformLocation(currentProgram.id, "numBoids"), numBoids);
    runComputeShader();

    glUseProgram(0);
}

void Boids::draw()
{
    if(applyLighting){
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
    } else {
        glDisable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);
    }

    Boid *boids = (Boid*)boidsData.map(GL_READ_WRITE);
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < numBoids; i++) {
        boids[i] = boids[numBoids + i];

        glColor3f(colors[i].r, colors[i].g, colors[i].b);
        for(int j = 0; j < 6*4; j += 4){
            Vector& a = boids[i].triangles[j + 0];
            Vector& b = boids[i].triangles[j + 1];
            Vector& c = boids[i].triangles[j + 2];
            Vector& n = boids[i].triangles[j + 3];
            glNormal3f(n.x, n.y, n.z);
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b.x, b.y, b.z);
            glVertex3f(c.x, c.y, c.z);
        }
    }
    glEnd();
    boidsData.unmap();
}

void Boids::setBoidSize(float width, float height)
{
    boidWidth = width;
    boidHeight = height;
    calculateBoidShape();
}

void Boids::calculateBoidShape()
{
    // calculates the vertices coordinates of the triangles of a pyramid pointing upwards (0, 1, 0)

    Vector top(0, boidHeight/2.f, 0);
    Vector bottom(0, -top.y, 0);

    Vector verts[5];

    float w = boidWidth/2.f;

    verts[0] = Vector(-w, bottom.y, -w);
    verts[1] = Vector(w, bottom.y, -w);
    verts[2] = Vector(w, bottom.y, w);
    verts[3] = Vector(-w, bottom.y, w);
    verts[4] = top;

    Vector triangles[6][3] = {
        {verts[0], verts[4], verts[1]},
        {verts[1], verts[4], verts[2]},
        {verts[2], verts[4], verts[3]},
        {verts[3], verts[4], verts[0]},
        {verts[0], verts[1], verts[3]},
        {verts[3], verts[1], verts[2]}
    };

    for(int i = 0; i < 6; i++){
        int index = i * 4;
        boidModelTriangles[index]   = triangles[i][0];
        boidModelTriangles[index+1] = triangles[i][1];
        boidModelTriangles[index+2] = triangles[i][2];
    }
}

void Boids::calculateRayDirs()
{
    // calculate evenly distributed points on a sphere
    // from : https://stackoverflow.com/questions/9600801/evenly-distributing-n-points-on-a-sphere/44164075#44164075

    rayDirs.resize(numRays * sizeof(Vector));
    Vector *rays = (Vector*)rayDirs.map(GL_WRITE_ONLY);
    float count = (float)std::max(numRays - 1, 1);
    for(int i = 0; i < numRays; i++){
        float t = (float)i / count;
        float inclination = acos(1.f - 2.f * t);
        float azimuth = 2.f * PI * PHI * (float)i;

        float x = sin(inclination) * cos(azimuth);
        float y = sin(inclination) * sin(azimuth);
        float z = cos(inclination);

        rays[i] = Vector(x, y, z);
    }
    rayDirs.unmap();
}

void Boids::createBuffers()
{
    boidsData = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, numBoids * 2 * sizeof(Boid));
    rayDirs = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_READ, numRays * sizeof(Vector));

    hasBuffers = true;
}

void Boids::createProgram()
{
    boidProgram = ComputeProgram("Boid.glsl", calculateOptimalDisptachSpace(numBoids, 1, 1));

    hasProgram = true;
}

void Boids::updateDispatchParams()
{
    boidProgram.dispatchParams = calculateOptimalDisptachSpace(numBoids, 1, 1);
}

void Boids::resizeBoidBuffer()
{
    boidsData.resize(numBoids * 2 * sizeof(Boid));
}

void Boids::deleteBuffers()
{
    boidsData.deleteBuffer();
    rayDirs.deleteBuffer();

    hasBuffers = false;
}

void Boids::deleteProgram()
{
    glDeleteProgram(boidProgram.id);
}

Boids::Boid::Boid()
{

}

Boids::Boid::Boid(vec3d _pos, vec3d _vel) : pos(Vector(_pos)), vel(Vector(_vel))
{

}

Boids::Vector::Vector()
{

}

Boids::Vector::Vector(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
{

}

Boids::Vector::Vector(vec3d v) : x(v.x), y(v.y), z(v.z)
{

}

Boids::~Boids()
{

}
