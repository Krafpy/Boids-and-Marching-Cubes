#ifndef CAMERA_H
#define CAMERA_H

#include <vec3d.h>


class Camera
{
    public:
        vec3d pos;
        vec3d center;
        float rotSpeed;
        float zoomSpeed;
        float translateSpeed;
        float minDist, maxDist;

        Camera();
        Camera(vec3d _pos, float _rotSpeed, float _zoomSpeed, float _translateSpeed, float _minDist, float _maxDist);
        void enableRotation();
        void disableRotation();
        void enableTranslation();
        void disableTranslation();

        void setup(vec3d _pos, float _rotSpeed, float _zoomSpeed, float _translateSpeed, float _minDist, float _maxDist);

        // Calculate camera eye coordinates
        void update();
        // Updates the position of the mouse used for other methods
        void updateMouseDetlas(float mouseX, float mouseY, float deltaTime);
        // Update spherical angles according to mouse movements
        void rotate();
        // Translates the camera center
        void translate();
        // Update the distance to the center of the scene (i.e. radius of spherical coordinates)
        void updateDistance(float offset);

        void resetCenter();

        virtual ~Camera();

    protected:

    private:
        bool isRotating = false, isTranslating = false;
        float prevMouseX = 0.f, prevMouseY = 0.f;
        float mouseDX = 0.f, mouseDY = 0.f;
        float heightAngle, deviationAngle; // respectively latitude and longitude
        float distanceFromCenter;
        vec3d translationDirX, translationDirY;

        void calculateTranslationDirs();

        void setup();
};

#endif // CAMERA_H
