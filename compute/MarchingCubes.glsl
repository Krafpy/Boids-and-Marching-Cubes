#version 460

#extension GL_ARB_compute_variable_group_size : enable

precision highp float;
precision highp int;

layout (local_size_variable) in;

layout (std430, binding = 0) buffer pointBuffer
{
    vec4 points[];
};

struct Vector
{
    float x, y, z;
};

layout (std430, binding = 1) buffer vertexBuffer
{
    int vertCount;
    Vector vertices[];
};

layout (std430, binding = 4) buffer normalsBuffer
{
    Vector normals[];
};

layout (std430, binding = 2) buffer cubesBuffer
{
    int cubes[];
};

layout (std430, binding = 3) buffer tablesBuffer
{
    int edgeTable[256];
};


uniform ivec3 densityGridDims;
uniform ivec3 cubeGridDims;
uniform float surfaceLevel;
uniform int normalsOffset;


// currently processed cube data
struct ControlNode
{
    vec3 pos;
    vec3 normal;
    float value;
};

ControlNode controlNodes[8];
int edgeNodes[12];
int configuration = 0;
int bordering = 0;


// bordering table : which edge vertices are necessary to create according
// to the relative position along the grid borders
const int bordTable[8][12] = {
    {0, 3, 8, -1, -1,  -1, -1, -1, -1, -1, -1, -1},    // 0, most case, i.e when x < w-1, y < h-1, z < d-1 calculate only three edge vertices
    {0, 3, 8, 2, 11, -1, -1, -1, -1, -1, -1, -1},      // 1
    {0, 3, 8, 9, 1,  -1, -1, -1, -1, -1, -1, -1},      // 2
    {0, 3, 8, 2, 11, 9, 1, 10, -1, -1, -1, -1},        // 3
    {0, 3, 8, 4, 7,  -1, -1, -1,  -1, -1, -1, -1},     // 4
    {0, 3, 8, 2, 11, 4, 7, 6, -1, -1, -1, -1},         // 5
    {0, 3, 8, 4, 7,  1, 9, 5, -1, -1, -1, -1},         // 6
    {0, 3, 8, 2, 11, 4, 7, 6, 5, 10, 9, 1}             // 7
};

const int numVerticesPerBordering[8] = {3, 5, 5, 8, 5, 8, 8, 12};

// edges:                0  1  2  3  4  5  6  7  8  9  10 11
const int edgeNodeA[] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3};
const int edgeNodeB[] = {1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7};


int indexPoint(int x, int y, int z){
    return z + densityGridDims.z * y + densityGridDims.z * densityGridDims.y * x;
}

int indexCube(int x, int y, int z){
    return (z + cubeGridDims.z * y + cubeGridDims.z * cubeGridDims.y * x) * 13;
}

ControlNode getControlNode(int x, int y, int z){
    int i = indexPoint(x, y, z);
    return ControlNode(points[i].xyz, vec3(normals[i].x, normals[i].y, normals[i].z), points[i].w);
}

// returns the vqlue used for vertex and normal interpolation
float tvalue(float v1, float v2){
    return (surfaceLevel - v1) / (v2 - v1);
}

void createEdgeVertex(int edgeLocalID){
    // Get the unique id for the new vertex, i.e. its position in the buffer
    int vertexID = atomicAdd(vertCount, 1);

    // Edge extremities where the new vertex belongs
    ControlNode nodeA = controlNodes[edgeNodeA[edgeLocalID]];
    ControlNode nodeB = controlNodes[edgeNodeB[edgeLocalID]];

    // calculate the interpolated coordinates and normal of the vertex
    float t = tvalue(nodeA.value, nodeB.value);
    vec3 p = mix(nodeA.pos, nodeB.pos, t);
    vec3 n = mix(nodeA.normal, nodeB.normal, t);

    // Append the new vertex into the vertices buffer
    vertices[vertexID] = Vector(p.x, p.y, p.z);
    vertices[normalsOffset+vertexID] = Vector(n.x, n.y, n.z);

    // Store the vertex id in the cube's edge location
    edgeNodes[edgeLocalID] = vertexID;
}

void main(){
    ivec3 id = ivec3(gl_GlobalInvocationID);
    int x = id.x;
    int y = id.y;
    int z = id.z;

    int currentCubeID = indexCube(x, y, z);

    // determine the cube control nodes, i.e. the cube's vertices
    // front face vertices
    controlNodes[0] = getControlNode(x, y, z);
    controlNodes[1] = getControlNode(x, y+1, z);
    controlNodes[2] = getControlNode(x+1, y+1, z);
    controlNodes[3] = getControlNode(x+1, y, z);
    // back face, directly opposite to the previous one
    controlNodes[4] = getControlNode(x, y, z+1);
    controlNodes[5] = getControlNode(x, y+1, z+1);
    controlNodes[6] = getControlNode(x+1, y+1, z+1);
    controlNodes[7] = getControlNode(x+1, y, z+1);

    // calculate the configuration of the cube
    for(int i = 0; i < 8; ++i){
        if(controlNodes[i].value > surfaceLevel){
            configuration |= 1 << i;
        }
    }

    if(configuration != 0 && configuration != 255){ // useless to continue if there are no vertices in the cube

        // calculate the bordering state of the cube, i.e. which extremities
        // of the grid the cube lays on
        if(x == cubeGridDims.x-1) bordering |= 1;
        if(y == cubeGridDims.y-1) bordering |= 2;
        if(z == cubeGridDims.z-1) bordering |= 4;

        // Calculate edges vertices

        // Create the necessary additional edges for cubes along borders of the grid, based
        // on its bordering value
        for(int i = 0; i < numVerticesPerBordering[bordering]; ++i){
            if((edgeTable[configuration] & (1 << bordTable[bordering][i])) != 0)
                createEdgeVertex(bordTable[bordering][i]);
        }

        // Pass the newly created vertices indexes to neighboring cubes to share the vertices

        if(x > 0){
            cubes[indexCube(x-1, y, z)+2] = edgeNodes[0];
            cubes[indexCube(x-1, y, z)+11] = edgeNodes[8];
            if(y > 0)
                cubes[indexCube(x-1, y-1, z)+10] = edgeNodes[8];
        }
        if(y > 0){
            cubes[indexCube(x, y-1, z)+1] = edgeNodes[3];
            cubes[indexCube(x, y-1, z)+9] = edgeNodes[8];
            if(z > 0)
                cubes[indexCube(x, y-1, z-1)+5] = edgeNodes[3];
        }
        if(z > 0){
            cubes[indexCube(x, y, z-1)+7] = edgeNodes[3];
            cubes[indexCube(x, y, z-1)+4] = edgeNodes[0];
            if(x > 0)
                cubes[indexCube(x-1, y, z-1)+6] = edgeNodes[0];
        }

        if(bordering != 0){
            if((bordering & 1) != 0){
                if(z > 0) cubes[indexCube(x, y, z-1)+6] = edgeNodes[2];
                if(y > 0) cubes[indexCube(x, y-1, z)+10] = edgeNodes[11];
            }
            if((bordering & 2) != 0){
                if(z > 0) cubes[indexCube(x, y, z-1)+5] = edgeNodes[1];
                if(x > 0) cubes[indexCube(x-1, y, z)+10] = edgeNodes[9];
            }
            if((bordering & 4) != 0){
                if(x > 0) cubes[indexCube(x-1, y, z)+6] = edgeNodes[4];
                if(y > 0) cubes[indexCube(x, y-1, z)+5] = edgeNodes[7];
            }
        }

        // finally store the current cube's edge vertices into the corresponding location in the buffer
        int edgeLocalID;
        for(int i = 0; i < numVerticesPerBordering[bordering]; ++i){
            edgeLocalID = bordTable[bordering][i];
            cubes[currentCubeID+edgeLocalID] = edgeNodes[edgeLocalID];
        }
    }
    cubes[currentCubeID+12] = configuration;
}
