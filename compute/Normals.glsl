#version 460

#extension GL_ARB_compute_variable_group_size : enable

precision highp float;
precision highp int;

layout (local_size_variable) in;

layout (std430, binding = 0) buffer densityBuffer
{
    vec4 points[];
};

struct Vector
{
    float x, y, z;
};

layout (std430, binding = 4) buffer normalsBuffer
{
    Vector normals[];
};

uniform ivec3 dims;

int index(int x, int y, int z){
    return z + dims.z * y + dims.z * dims.y * x;
}

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID);
    int x = id.x;
    int y = id.y;
    int z = id.z;

    int i = index(x, y, z);

    // calculate the normals at each point by deriving the neighboring points
    // points on the edges have their components set to their density value

    float v = points[i].w;
    float dx = v;
    float dy = v;
    float dz = v;

    if(x > 0 && x < dims.x-1) dx = points[index(x-1, y, z)].w - points[index(x+1, y, z)].w;
    if(y > 0 && y < dims.y-1) dy = points[index(x, y-1, z)].w - points[index(x, y+1, z)].w;
    if(z > 0 && z < dims.z-1) dz = points[index(x, y, z-1)].w - points[index(x, y, z+1)].w;

    vec3 n = normalize(vec3(dx, dy, dz));

    normals[i] = Vector(n.x, n.y, n.z);
}
