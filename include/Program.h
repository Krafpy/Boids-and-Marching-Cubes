#ifndef PROGRAM_H
#define PROGRAM_H

#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glfw3.h>

#include <ConfigParser.h>
#include <MarchingCubes.h>
#include <Boids.h>
#include <Camera.h>


class Program
{
    public:
        GLFWwindow *window;

        Program(GLFWwindow *_window, ConfigParser& _config);

        void setup();
        void update();

        // GLFW event callbacks
        void onKeyPressed(int key, int scancode, int action, int mods);
        void onCursorPosition(double x, double y);
        void onMouseButton(int button, int action, int mods);
        void onScrollRoll(double xoff, double yoff);

        virtual ~Program();

    protected:

    private:
        int winWidth, winHeight;

        ConfigParser config;
        MarchingCubes mesh;
        Boids boids;
        Camera cam;

        bool isStarting = true;

        bool meshEnabled = true;
        bool meshWasResized = false;
        bool meshHasGeneration = false;
        bool randomizeOnGeneration = true;

        bool pauseBoids = false;
        bool numBoidsChanged = false;

        float frameTime = 0.f;

        struct Feature
        {
            float x, y, z;
            bool enabled;
            virtual void draw();
        };

        struct Box : Feature
        {
            void draw() override;
        } box;

        struct Axes : Feature
        {
            float len;
            void draw() override;
        } axes;

        // mesh material and lighting
        const GLfloat light_ambient[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
        const GLfloat light_diffuse[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
        const GLfloat light_specular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const GLfloat light_position[4] = { 1.0f, 1.0f, -1.0f, 0.0f };

        const GLfloat mat_ambient[4]    = { 0.7f, 0.7f, 0.7f, 1.0f };
        const GLfloat mat_diffuse[4]    = { 0.8f, 0.8f, 0.8f, 1.0f };
        const GLfloat mat_specular[4]   = { 0.5f, 0.5f, 0.5f, 1.0f };
        const GLfloat high_shininess[1] = { 100.0f };

        void initEnables();
        void initLight();
        void initMaterial();

        void configureProgram();
        void configureMesh();
        void configureBoids();

        void setupCamera();
        void configureCamera();

        void resizeFeatures();

        void resetProjectionSettings();

        void generateMesh();
};

#endif // PROGRAM_H
