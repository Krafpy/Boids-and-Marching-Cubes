#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glfw3.h>

#include <stdlib.h>
#include <iostream>
#include <time.h>

#include <Program.h>


// GLFW event callbacks
static void onKeyPressed(GLFWwindow *window, int key, int scancode, int action, int mods);
static void onCursorPosition(GLFWwindow *window, double x, double y);
static void onMouseButton(GLFWwindow *window, int button, int action, int mods);
static void onScrollRoll(GLFWwindow *window, double xoff, double yoff);
static void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

/* Program entry point */

int main(int argc, char *argv[])
{
    srand(time(0));

    ConfigParser config("config.txt");
    //config.printData();

    if(!glfwInit()){
        glfwTerminate();
        return 0;
    }

    GLFWwindow *window;

    window = glfwCreateWindow(600, 400, "Boids & Marching Cubes", NULL, NULL);

    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, onKeyPressed);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursorPosCallback(window, onCursorPosition);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetScrollCallback(window, onScrollRoll);
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, 1);

    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK)
    {
        glfwTerminate();
        return 0;
    }

    // enable/disable debug output
    if(0){
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback( MessageCallback, 0 );
    }

    // create the current program
    Program current(window, config);
    glfwSetWindowUserPointer(window, &current);

    current.setup();

    while(!glfwWindowShouldClose(window))
    {
        current.update();
    }

	glfwTerminate();

    return EXIT_SUCCESS;
}

static Program& getCurrent(GLFWwindow *window)
{
    return *(Program*)glfwGetWindowUserPointer(window);
}

static void onKeyPressed(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    getCurrent(window).onKeyPressed(key, scancode, action, mods);
}

static void onCursorPosition(GLFWwindow *window, double x, double y)
{
    getCurrent(window).onCursorPosition(x, y);
}

static void onMouseButton(GLFWwindow *window, int button, int action, int mods)
{
    getCurrent(window).onMouseButton(button, action, mods);
}

static void onScrollRoll(GLFWwindow *window, double xoff, double yoff)
{
    getCurrent(window).onScrollRoll(xoff, yoff);
}

static void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
  fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}



