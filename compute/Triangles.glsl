#version 460

#extension GL_ARB_compute_variable_group_size : enable

precision highp float;
precision highp int;

layout (local_size_variable) in;

layout (std430, binding = 2) buffer cubesBuffer
{
    int cubes[];
};

layout (std430, binding = 3) buffer tablesBuffer
{
    int edgeTable[256];
    int triTable[256][16];
};

struct Vector
{
    float x, y, z;
};

layout (std430, binding = 1) buffer verticesBuffer
{
    Vector vertices[];
};

struct Triangle
{
    int a, b, c;
};

layout (std430, binding = 5) coherent buffer trianglesBuffer
{
    int triCount;
    Triangle triangles[];
};


vec3 getVertex(int vertex){
    Vector v = vertices[vertex];
    return vec3(v.x, v.y, v.z);
}

void main(){
    int id = int(gl_GlobalInvocationID.x);
    int cubeIndex = id * 13;

    // Calculate the triangles of the cube

    int configuration = cubes[cubeIndex+12];
    int triConfigIndex = configuration;

    for(int i = 0; triTable[triConfigIndex][i] != -1; i += 3){
        int vertA = cubes[cubeIndex + triTable[triConfigIndex][i]];
        int vertB = cubes[cubeIndex + triTable[triConfigIndex][i+1]];
        int vertC = cubes[cubeIndex + triTable[triConfigIndex][i+2]];

        // Append the triangle in the triangles buffer
        int triIndex = atomicAdd(triCount, 1);
        triangles[triIndex] = Triangle(vertA, vertB, vertC);
    }
}
