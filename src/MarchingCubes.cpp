#include "MarchingCubes.h"

#include <iostream>
#include <Tables.h>


#define NOISE_SSB_BP        0
#define VERTICES_SSB_BP     1
#define CUBES_SSB_BP        2
#define TRITABLES_SSB_BP    3
#define NORMALS_SSB_BP      4
#define TRIANGLES_SSB_BP    5


MarchingCubes::MarchingCubes()
{

}

MarchingCubes::MarchingCubes(int width, int height, int depth, float _cubeSize) :
    cubeSize(_cubeSize),
    densityGrid(width+1, height+1, depth+1),
    cubeGrid(width, height, depth)
{
    resize();
}

void MarchingCubes::resize()
{
    int x = cubeGrid.x, y = cubeGrid.y, z = cubeGrid.z;
    // We can deduce the maximum number of vertices the grid can have by counting
    // the number of edges in the grid, which gives a result just below 3*(w+1)*(h+1)*(d+1)
    maxNumVertices = y*(x+1)*(z+1) + (y+1)*x*(z+1) + (y+1)*(x+1)*z;
    maxNumTriangles = cubeGrid.count * 5; // each cube can have 5 triangles at most

    size.x = cubeGrid.x * cubeSize;
    size.y = cubeGrid.y * cubeSize;
    size.z = cubeGrid.z * cubeSize;

    size.max = std::max(size.x, std::max(size.y, size.z));
}

bool MarchingCubes::resize(int width, int height, int depth, float _cubeSize)
{
    bool gridChange = width != cubeGrid.x || height != cubeGrid.y || depth != cubeGrid.z;
    bool cubeChange = cubeSize != _cubeSize;

    densityGrid = Volume(width+1, height+1, depth+1);
    cubeGrid = Volume(width, height, depth);
    cubeSize = _cubeSize;

    resize();

    if(gridChange){
        if(hasPrograms){
            updateDispatchParams();
        } else {
            createPrograms();
        }
        if(hasBuffers){
            deleteBuffers();
        }
        createBuffers();
    }

    // returns true if the overall mesh size is different compared to the previous one
    return gridChange || cubeChange;
}

void MarchingCubes::generate()
{
    // Start recording the generation process duration
    startDurationRecording();

    // Set buffers binding points
    density.setBindingPoint(NOISE_SSB_BP);
    normals.setBindingPoint(NORMALS_SSB_BP);
    cubes.setBindingPoint(CUBES_SSB_BP);
    vertices.setBindingPoint(VERTICES_SSB_BP);
    triangles.setBindingPoint(TRIANGLES_SSB_BP);
    tables.setBindingPoint(TRITABLES_SSB_BP);

    // Reset the counters to 0
    GLuint zero = 0;
    vertices.setSubData(0, sizeof(GLuint), &zero);
    triangles.setSubData(0, sizeof(GLuint), &zero);

    // Generate the density field
    useProgram(densityCompute);
    glUniform3i(glGetUniformLocation(currentProgram.id, "dims"), densityGrid.x, densityGrid.y, densityGrid.z);
    glUniform1f(glGetUniformLocation(currentProgram.id, "cubeSize"), cubeSize);
    glUniform3f(glGetUniformLocation(currentProgram.id, "offset"), noise.offset.x, noise.offset.y, noise.offset.z);
    glUniform1i(glGetUniformLocation(currentProgram.id, "octaves"), noise.octaves);
    glUniform1f(glGetUniformLocation(currentProgram.id, "noiseScale"), noise.noiseScale);
    glUniform1f(glGetUniformLocation(currentProgram.id, "lacunarity"), noise.lacunarity);
    glUniform1f(glGetUniformLocation(currentProgram.id, "persistence"), noise.persistence);
    glUniform1f(glGetUniformLocation(currentProgram.id, "noiseWeight"), noise.noiseWeight);
    glUniform1f(glGetUniformLocation(currentProgram.id, "floorOffset"), noise.floorOffset);
    glUniform1i(glGetUniformLocation(currentProgram.id, "closeEdges"), noise.closeEdges);
    glUniform1f(glGetUniformLocation(currentProgram.id, "hardFloor"), noise.hardFloor);
    glUniform1f(glGetUniformLocation(currentProgram.id, "floorWeight"), noise.floorWeight);
    glUniform1f(glGetUniformLocation(currentProgram.id, "stepSize"), noise.stepSize);
    glUniform1f(glGetUniformLocation(currentProgram.id, "stepWeight"), noise.stepWeight);
    runComputeShader();

    // avoid having small shapes
    if(minRegionSize > 0)
        removeSmallRegions();

    // Generate the normals for each density point
    useProgram(normalsCompute);
    glUniform3i(glGetUniformLocation(currentProgram.id, "dims"), densityGrid.x, densityGrid.y, densityGrid.z);
    runComputeShader();

    // Marching cubes compute shader
    useProgram(marchingCubesCompute);
    glUniform3i(glGetUniformLocation(currentProgram.id, "densityGridDims"), densityGrid.x, densityGrid.y, densityGrid.z);
    glUniform3i(glGetUniformLocation(currentProgram.id, "cubeGridDims"), cubeGrid.x, cubeGrid.y, cubeGrid.z);
    glUniform1f(glGetUniformLocation(currentProgram.id, "surfaceLevel"), surfaceLevel);
    glUniform1i(glGetUniformLocation(currentProgram.id, "normalsOffset"), maxNumVertices);
    runComputeShader();

    // Triangulation process
    useProgram(trianglesCompute);
    runComputeShader();

    glUseProgram(0);

    // Retrieve the number of generated vertices and triangles
    vertices.getSubData(0, sizeof(GLuint), &numVertices);
    triangles.getSubData(0, sizeof(GLuint), &numTriangles);

    // Stop recording generation time
    generationDuration = endDurationRecording();
}

MarchingCubes::Coord::Coord(int _i, int _j, int _k) : i(_i), j(_j), k(_k)
{

}

int MarchingCubes::pointIndex(Coord c)
{
    return c.k + densityGrid.z * c.j + densityGrid.z * densityGrid.y * c.i;
}

void MarchingCubes::removeSmallRegions()
{
    // 3D floodfill algorithm to detect regions of points with values above the surface level (i.e. solid regions)
    // and sets the point density value under the surface level to ignore it if the region size is under minRegionSize

    Point *points = (Point*)density.map(0, densityGrid.count * sizeof(Point), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    std::vector<bool> visited(densityGrid.count, false);

    for(int i = 0; i < densityGrid.x; i++){
        for(int j = 0; j < densityGrid.y; j++){
            for(int k = 0; k < densityGrid.z; k++){

                Coord c(i, j, k);
                int index = pointIndex(c);

                if(!visited[index] && points[index].val > surfaceLevel){
                    std::vector<int> region;

                    getRegionPoints(c, points, visited, region);

                    if(region.size() < (unsigned int)minRegionSize){
                        for(auto it = region.begin(); it != region.end(); ++it)
                            points[*it].val = surfaceLevel - 1.f;
                    }
                }
            }
        }
    }

    density.unmap();
}

void MarchingCubes::getRegionPoints(Coord start, Point* points, std::vector<bool>& visited, std::vector<int>& region)
{
    int startIndex = pointIndex(start);
    visited[startIndex] = true;
    region.push_back(startIndex);

    std::queue<Coord> q;
    q.push(start);

    while(!q.empty()){
        Coord c = q.front();
        q.pop();

        appendRegionPoint(Coord(c.i-1, c.j, c.k), points, visited, region, q);
        appendRegionPoint(Coord(c.i+1, c.j, c.k), points, visited, region, q);
        appendRegionPoint(Coord(c.i, c.j-1, c.k), points, visited, region, q);
        appendRegionPoint(Coord(c.i, c.j+1, c.k), points, visited, region, q);
        appendRegionPoint(Coord(c.i, c.j, c.k-1), points, visited, region, q);
        appendRegionPoint(Coord(c.i, c.j, c.k+1), points, visited, region, q);
    }
}

void MarchingCubes::appendRegionPoint(Coord c, Point* points, std::vector<bool>& visited, std::vector<int>& region, std::queue<Coord>& q)
{
    if(c.i >= 0 && c.i < densityGrid.x && c.j >= 0 && c.j <= densityGrid.y && c.k >= 0 && c.k < densityGrid.z){
        int index = pointIndex(c);
        if(!visited[index] && points[index].val > surfaceLevel){
            visited[index] = true;
            region.push_back(index);
            q.push(c);
        }
    }
}

Buffer* MarchingCubes::getCubesBuffer()
{
    return &cubes;
}

float MarchingCubes::getCubeSize()
{
    return cubeSize;
}

MarchingCubes::Volume MarchingCubes::getCubeGrid()
{
    return cubeGrid;
}

void MarchingCubes::draw()
{
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);

    glColor3f(color.r, color.g, color.b);

    glBindBuffer(GL_ARRAY_BUFFER, vertices.id);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, (void*)(sizeof(GLuint)));

        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, (void*)(sizeof(float)*3*maxNumVertices+sizeof(GLuint)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangles.id);
        glDrawElements(GL_TRIANGLES, numTriangles*3, GL_UNSIGNED_INT, (void*)(sizeof(GLuint)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
}

void MarchingCubes::updateDispatchParams()
{
    densityCompute.dispatchParams = calculateOptimalDisptachSpace(densityGrid.x, densityGrid.y, densityGrid.z);
    normalsCompute.dispatchParams = densityCompute.dispatchParams;
    marchingCubesCompute.dispatchParams = calculateOptimalDisptachSpace(cubeGrid.x, cubeGrid.y, cubeGrid.z);
    trianglesCompute.dispatchParams = calculateOptimalDisptachSpace(cubeGrid.count, 1, 1);
}

void MarchingCubes::createPrograms()
{
    DispatchParams densityDispatch = calculateOptimalDisptachSpace(densityGrid.x, densityGrid.y, densityGrid.z);
    DispatchParams marchingCubesDispatch = calculateOptimalDisptachSpace(cubeGrid.x, cubeGrid.y, cubeGrid.z);
    DispatchParams trianglesDispatch = calculateOptimalDisptachSpace(cubeGrid.count, 1, 1);

    densityCompute = ComputeProgram("Density.glsl", densityDispatch);
    normalsCompute = ComputeProgram("Normals.glsl", densityDispatch);
    marchingCubesCompute = ComputeProgram("MarchingCubes.glsl", marchingCubesDispatch);
    trianglesCompute = ComputeProgram("Triangles.glsl", trianglesDispatch);

    hasPrograms = true;
}

void MarchingCubes::createBuffers()
{
    // Generate the density grid for noise
    density = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, densityGrid.count * 4 * sizeof(float));

    // Generate the normals buffer
    normals = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, densityGrid.count * 3 * sizeof(float));

    // Generate the cubes buffer
    cubes = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, cubeGrid.count * 13 * sizeof(int));

    // Generate the vertices buffer
    vertices = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, maxNumVertices * 2 * 3 * sizeof(float) + sizeof(GLuint));
    // Generate the triangles buffer
    triangles = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, maxNumTriangles * 3 * sizeof(int) + sizeof(GLuint));

    // Load the edge and triangulation table into a buffer
    int *flatTriTable = flattenTriTable();
    tables = Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW, (256+256*16)*sizeof(int));
    tables.setSubData(0, sizeof(edgeTable), edgeTable);
    tables.setSubData(sizeof(edgeTable), 256*16*sizeof(int), flatTriTable);
    delete[] flatTriTable;

    hasBuffers = true;
}

int* MarchingCubes::flattenTriTable()
{
    int *flatTriTable = new int[256*16];
    int flatTriIndex = 0;
    for(int i = 0; i < 256; i++){
        for(int j = 0; j < 16; j++){
            flatTriTable[flatTriIndex] = triTable[i][j];
            flatTriIndex++;
        }
    }
    return flatTriTable;
}

void MarchingCubes::deletePrograms()
{
    glDeleteProgram(densityCompute.id);
    glDeleteProgram(normalsCompute.id);
    glDeleteProgram(marchingCubesCompute.id);
    glDeleteProgram(trianglesCompute.id);

    hasPrograms = false;
}

void MarchingCubes::deleteBuffers()
{
    cubes.deleteBuffer();
    density.deleteBuffer();
    tables.deleteBuffer();
    triangles.deleteBuffer();
    vertices.deleteBuffer();
    normals.deleteBuffer();

    numVertices = 0;
    numTriangles = 0;

    hasBuffers = false;
}

MarchingCubes::~MarchingCubes()
{

}
