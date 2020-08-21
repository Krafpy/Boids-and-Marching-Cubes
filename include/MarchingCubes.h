#ifndef MARCHINGCUBES_H
#define MARCHINGCUBES_H

#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glfw3.h>

#include <ComputeProcess.h>

#include <NoiseSettings.h>
#include <queue>
#include <vector>


class MarchingCubes : private ComputeProcess
{
    public:
        int numVertices = 0;
        int numTriangles = 0;
        float generationDuration = 0;
        float surfaceLevel = 0.f;
        int minRegionSize = 1000;

        NoiseSettings noise;

        struct {
            float x, y, z, max;
        } size;

        struct {
            float r = 1.f, g = 1.f, b = 1.f;
        } color;

        MarchingCubes();
        MarchingCubes(int width, int height, int depth, float _cubeSize);

        bool resize(int width, int height, int depth, float _cubeSize);

        void generate();
        void draw();

        void createPrograms();
        void createBuffers();
        void deletePrograms();
        void deleteBuffers();

        Buffer* getCubesBuffer();
        float getCubeSize();
        Volume getCubeGrid();

        virtual ~MarchingCubes();

    protected:

    private:
        float cubeSize = 0.1f;
        int maxNumVertices;
        int maxNumTriangles;

        bool hasPrograms = false, hasBuffers = false;

        ComputeProgram densityCompute;
        ComputeProgram normalsCompute;
        ComputeProgram marchingCubesCompute;
        ComputeProgram trianglesCompute;

        Buffer density;
        Buffer normals;
        Buffer cubes;
        Buffer vertices;
        Buffer triangles;
        Buffer tables;

        Volume densityGrid, cubeGrid;

        int* flattenTriTable();

        void resize();
        void updateDispatchParams();

        struct Coord
        {
            int i, j, k;
            Coord(int _i, int _j, int _k);
        };

        struct Point
        {
            float x, y, z, val;
        };

        void removeSmallRegions();
        void getRegionPoints(Coord start, Point* points, std::vector<bool>& visited, std::vector<int>& region);
        void appendRegionPoint(Coord c, Point* points, std::vector<bool>& visited, std::vector<int>& region, std::queue<Coord>& q);
        int pointIndex(Coord c);
};

#endif // MARCHINGCUBES_H
