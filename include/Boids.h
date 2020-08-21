#ifndef BOIDS_H
#define BOIDS_H

#include <ComputeProcess.h>
#include <MarchingCubes.h>
#include <vec3d.h>


class Boids : private ComputeProcess
{
    public:
        int numBoids = 0;

        struct Color
        {
            float r = 1.f, g = 1.f, b = 1.f;
        };

        Color mainColor;
        float colorDeviation = 0.1f;

        struct {
            float x, y, z;
        } box;

        float predictionLength = 0.1f;
        float minSpeed = 0.01f;
        float maxSpeed = 1.f;
        float maxForce = 0.1f;
        float viewRadius = 1.f;
        float viewAngle = 3.141592f;
        float avoidRadius = 0.1f;
        float cohesionCoef = 1.f;
        float alignmentCoef = 1.f;
        float separationCoef = 1.f;
        float obstacleCoef = 1.f;

        Buffer *cubes;
        Volume cubeGrid;
        float cubeSize;
        bool avoidMesh = false;

        bool applyLighting = true;

        Boids();
        Boids(int _numBoids, float width, float height, int _numRays);

        void createProgram();
        void createBuffers();
        void deleteProgram();
        void deleteBuffers();

        bool setup(int _numBoids, float width, float height, int _numRays);
        void update(float deltaTime);
        void draw();

        void generateBoids();

        virtual ~Boids();

    protected:

    private:
        float boidWidth = 1.f, boidHeight = 1.f;

        struct Vector
        {
            float x = 0.f, y = 0.f, z = 0.f;

            Vector();
            Vector(float _x, float _y, float _z);
            Vector(vec3d v);
        };

        struct Boid
        {
            Vector pos, vel;
            Vector triangles[6*4];

            Boid();
            Boid(vec3d _pos, vec3d _vel);
        };

        Vector boidModelTriangles[6*4];

        ComputeProgram boidProgram;
        Buffer boidsData;
        Buffer rayDirs;

        int numRays = 0;

        bool hasBuffers = false, hasProgram = false;
        bool numBoidsChanged = true, numRayDirsChanged = true;

        Color *colors;
        bool colorsArrayExist = false;

        void setup();

        void calculateBoidShape();
        void calculateRayDirs();
        void setBoidSize(float width, float height);

        void updateDispatchParams();
        void resizeBoidBuffer();
};

#endif // BOIDS_H
