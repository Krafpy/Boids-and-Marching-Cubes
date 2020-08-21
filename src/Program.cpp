#include "Program.h"

#include <stdlib.h>


Program::Program(GLFWwindow *_window, ConfigParser& _config) : window(_window), config(_config)
{
    configureProgram();

    initEnables();
    initLight();
    initMaterial();

    configureMesh();
    configureBoids();

    setupCamera();
}

void Program::configureMesh()
{
    // Noise configuration

    if(config.exist("offsetSeed")){
        mesh.noise.seed(config.getInt("offsetSeed"));
    } else {
        mesh.noise.seed(rand());
    }

    mesh.noise.noiseScale = config.getFloat("noiseScale");
    mesh.noise.lacunarity = config.getFloat("lacunarity");
    mesh.noise.persistence = config.getFloat("persistence");
    mesh.noise.octaves = config.getInt("octaves");

    mesh.noise.closeEdges = config.getBool("closeEdges");

    mesh.noise.stepSize = config.getFloat("stepSize");
    mesh.noise.stepWeight = config.getFloat("stepWeight");

    mesh.noise.floorOffset = config.getFloat("floorOffset");
    mesh.noise.hardFloor = config.getFloat("hardFloor");
    mesh.noise.floorWeight = config.getFloat("floorWeight");

    mesh.noise.noiseWeight = config.getFloat("noiseWeight");

    // Mesh configuration

    mesh.surfaceLevel = config.getFloat("surfaceLevel");
    mesh.minRegionSize = config.getInt("minRegionSize");

    mesh.color.r = config.getFloat("meshColorR");
    mesh.color.g = config.getFloat("meshColorG");
    mesh.color.b = config.getFloat("meshColorB");

    int numCubesX = config.getInt("numCubesX");
    int numCubesY = config.getInt("numCubesY");
    int numCubesZ = config.getInt("numCubesZ");
    float cubeSize = config.getFloat("cubeSize");

    meshWasResized = mesh.resize(numCubesX, numCubesY, numCubesZ, cubeSize);

    resizeFeatures();
}

void Program::configureBoids()
{
    boids.mainColor.r = config.getFloat("boidColorR");
    boids.mainColor.g = config.getFloat("boidColorG");
    boids.mainColor.b = config.getFloat("boidColorB");

    boids.predictionLength = config.getFloat("predictionLength");

    boids.maxForce = config.getFloat("maxForce");
    boids.minSpeed = config.getFloat("minSpeed");
    boids.maxSpeed = config.getFloat("maxSpeed");
    boids.viewAngle = config.getFloat("viewAngle") * 3.141592 / 180.f; // convert degrees to radians
    boids.viewRadius = config.getFloat("viewRadius");
    boids.avoidRadius = config.getFloat("avoidRadius");

    boids.cohesionCoef = config.getFloat("cohesionCoef");
    boids.alignmentCoef = config.getFloat("alignmentCoef");
    boids.separationCoef = config.getFloat("separationCoef");
    boids.obstacleCoef = config.getFloat("obstacleCoef");

    boids.applyLighting = config.getBool("applyLightingOnBoids");
    boids.colorDeviation = config.getFloat("boidColorDeviation");

    float width = config.getFloat("boidWidth");
    float height = config.getFloat("boidHeight");
    int numBoids = config.getInt("numBoids");
    int numRayDirs = config.getInt("numRayDirs");

    boids.box.x = mesh.size.x;
    boids.box.y = mesh.size.y;
    boids.box.z = mesh.size.z;

    boids.cubes = mesh.getCubesBuffer();
    boids.cubeSize = mesh.getCubeSize();
    boids.cubeGrid = mesh.getCubeGrid();

    boids.avoidMesh = meshEnabled;

    numBoidsChanged = boids.setup(numBoids, width, height, numRayDirs);
}

void Program::configureCamera()
{
    cam.rotSpeed = config.getFloat("rotSpeed");
    cam.zoomSpeed = config.getFloat("zoomSpeed");
    cam.translateSpeed = config.getFloat("translateSpeed");
    cam.maxDist = std::max(mesh.size.max * 2.f, cam.maxDist);
}

void Program::setupCamera()
{
    float rotSpeed = config.getFloat("rotSpeed");
    float zoomSpeed = config.getFloat("zoomSpeed");
    float translateSpeed = config.getFloat("translateSpeed");
    cam.setup(vec3d(0, 0, -mesh.size.z * 2.f), rotSpeed, zoomSpeed, translateSpeed, 0.1f, mesh.size.max * 2.f);
}

void Program::configureProgram()
{
    if(config.exist("randomizeOnGeneration"))
        randomizeOnGeneration = config.getBool("randomizeOnGeneration");

    if(isStarting){
        if(config.exist("enableMeshOnStart"))
            meshEnabled = config.getBool("enableMeshOnStart");

        isStarting = false;
    }
}

void Program::resizeFeatures()
{
    // get box size for wire box and axes dimensions
    box.x = mesh.size.x / 2.f;
    box.y = mesh.size.y / 2.f;
    box.z = mesh.size.z / 2.f;

    axes.x = -box.x * 1.1f;
    axes.y = -box.y * 1.1f;
    axes.z = -box.z * 1.1f;

    axes.len = mesh.size.max * 0.2f;
}

void Program::setup()
{
    box.enabled = true;
    axes.enabled = false;

    if(meshEnabled)
        generateMesh();

    boids.generateBoids();

    glfwSetTime(0.0);
}

void Program::update()
{
    resetProjectionSettings();

    cam.update();
    gluLookAt(cam.pos.x, cam.pos.y, cam.pos.z, cam.center.x, cam.center.y, cam.center.z, 0.f, 1.f, 0.f);

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    if(!pauseBoids)
        boids.update(frameTime);
    boids.draw();

    if(meshEnabled)
        mesh.draw();

    if(axes.enabled)
        axes.draw();
    if(box.enabled)
        box.draw();

    glfwSwapBuffers(window);

    glfwPollEvents();

    frameTime = glfwGetTime();
    glfwSetTime(0.0);
}

void Program::generateMesh()
{
    printf("Generating new mesh...");

    if(randomizeOnGeneration)
        mesh.noise.seed(rand());

    // override random offsets if user defined ones exist
    if(config.exist("offsetX"))
        mesh.noise.offset.x = config.getFloat("offsetX");
    if(config.exist("offsetY"))
        mesh.noise.offset.y = config.getFloat("offsetY");
    if(config.exist("offsetZ"))
        mesh.noise.offset.z = config.getFloat("offsetZ");

    mesh.generate();

    meshWasResized = false;
    meshHasGeneration = true;

    printf("\rGenerated new mesh: seed: %d - vertices: %d, triangles: %d - %fms\n",
    mesh.noise.offsetSeed,
    mesh.numVertices, mesh.numTriangles,
    mesh.generationDuration);
}

void Program::Box::draw()
{
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
    glColor3f(1.f, 1.f, 1.f);
    glPointSize(1.f);
    glBegin(GL_LINE_LOOP);
        glVertex3f(-x, -y, -z);
        glVertex3f( x, -y, -z);
        glVertex3f( x,  y, -z);
        glVertex3f(-x,  y, -z);
    glEnd();
    glBegin(GL_LINE_LOOP);
        glVertex3f(-x, -y,  z);
        glVertex3f( x, -y,  z);
        glVertex3f( x,  y,  z);
        glVertex3f(-x,  y,  z);
    glEnd();
    glBegin(GL_LINES);
        glVertex3f(-x, -y, -z);
        glVertex3f(-x, -y,  z);
        glVertex3f( x, -y, -z);
        glVertex3f( x, -y,  z);
        glVertex3f(-x,  y, -z);
        glVertex3f(-x,  y,  z);
        glVertex3f( x,  y, -z);
        glVertex3f( x,  y,  z);
    glEnd();
}

void Program::Axes::draw()
{
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
    glColor3f(1.f, 1.f, 1.f);
    glPointSize(1.f);
    glBegin(GL_LINES);
        glColor3f(1.f, 0.f, 0.f);
        glVertex3f(x,     y, z);
        glVertex3f(x+len, y, z);

        glColor3f(0.f, 1.f, 0.f);
        glVertex3f(x, y,     z);
        glVertex3f(x, y+len, z);

        glColor3f(0.f, 0.f, 1.f);
        glVertex3f(x, y, z);
        glVertex3f(x, y, z+len);
    glEnd();
}

void Program::resetProjectionSettings()
{
    glfwGetWindowSize(window, &winWidth, &winHeight );
    winHeight = winHeight > 0 ? winHeight : 1;

    glViewport( 0, 0, winWidth, winHeight );

    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(65.0f, (GLfloat)winWidth/(GLfloat)winHeight, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Program::initEnables()
{
    glClearColor(0, 0, 0, 1);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable( GL_LINE_SMOOTH );
    glEnable( GL_POINT_SMOOTH );
    glEnable( GL_BLEND );

    glEnable(GL_RESCALE_NORMAL);

    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
}

void Program::initLight()
{
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
}

void Program::initMaterial()
{
    glMaterialfv(GL_FRONT, GL_AMBIENT,   mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
}

void Program::onKeyPressed(int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS){
        switch(key){

        case GLFW_KEY_A:
            axes.enabled = !axes.enabled;
            break;

        case GLFW_KEY_B:
            box.enabled = !box.enabled;
            break;

        case GLFW_KEY_SPACE:
            if(meshEnabled)
                generateMesh();
            boids.generateBoids();
            boids.avoidMesh = meshEnabled;
            break;

        case GLFW_KEY_C:
            cam.resetCenter();
            break;

        case GLFW_KEY_R:
            config.parse();
            configureProgram();
            configureMesh();
            configureCamera();
            configureBoids();
            if(meshWasResized || numBoidsChanged)
                boids.generateBoids();
            if(meshWasResized && meshEnabled)
                generateMesh();
            break;

        case GLFW_KEY_D:
            meshEnabled = !meshEnabled;
            if(meshEnabled && (!meshHasGeneration || meshWasResized)){
                generateMesh();
                boids.generateBoids();
            }
            boids.avoidMesh = meshEnabled;
            break;

        case GLFW_KEY_P:
            pauseBoids = !pauseBoids;
            break;

        default:
            break;
        }
    }
}

void Program::onCursorPosition(double x, double y)
{
    cam.updateMouseDetlas((float)x, (float)y, frameTime);
    cam.rotate();
    cam.translate();
}

void Program::onMouseButton(int button, int action, int mods)
{
    if(button == GLFW_MOUSE_BUTTON_LEFT){

        if(action == GLFW_PRESS)
            cam.enableRotation();
        else
            cam.disableRotation();

    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {

        if(action == GLFW_PRESS)
            cam.enableTranslation();
        else
            cam.disableTranslation();

    }
}

void Program::onScrollRoll(double xoff, double yoff)
{
    cam.updateDistance((float)yoff);
}

Program::~Program()
{
    mesh.deleteBuffers();
    mesh.deletePrograms();
    boids.deleteBuffers();
    boids.deleteProgram();
}
