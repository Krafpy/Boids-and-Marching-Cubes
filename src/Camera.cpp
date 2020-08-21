#include "Camera.h"

#include <vec3d.h>
#include <math.h>
#include <iostream>

#define PI 3.141592f


Camera::Camera()
{

}

Camera::Camera(vec3d _pos, float _rotSpeed, float _zoomSpeed, float _translateSpeed, float _minDist, float _maxDist) :
    pos(_pos), rotSpeed(_rotSpeed), zoomSpeed(_zoomSpeed), translateSpeed(_translateSpeed), minDist(_minDist), maxDist(_maxDist)
{
    setup();
}

void Camera::setup()
{
    // setup the camera and calculate start angles and distance

    deviationAngle = vec3d::angleBetween(vec3d(pos.x, 0, pos.z), vec3d(0, 0, -1));
    heightAngle = vec3d::angleBetween(vec3d(0, pos.y, pos.z), vec3d(0, 0, -1));

    center = vec3d();
    distanceFromCenter = vec3d::distance(pos, center);
}

void Camera::setup(vec3d _pos, float _rotSpeed, float _zoomSpeed, float _translateSpeed, float _minDist, float _maxDist)
{
    pos = _pos;
    rotSpeed = _rotSpeed;
    zoomSpeed = _zoomSpeed;
    translateSpeed = _translateSpeed;
    minDist = _minDist;
    maxDist = _maxDist;
    setup();
}

void Camera::updateMouseDetlas(float mouseX, float mouseY, float deltaTime)
{
    mouseDX = (mouseX - prevMouseX) * deltaTime;
    mouseDY = (mouseY - prevMouseY) * deltaTime;
    prevMouseX = mouseX;
    prevMouseY = mouseY;
}

void Camera::rotate()
{
    if(isRotating){
        float dx = mouseDX * rotSpeed;
        float dy = mouseDY * rotSpeed;

        deviationAngle += dx;
        heightAngle += dy;

        if(deviationAngle < -PI)
            deviationAngle = PI;
        else if (deviationAngle > PI)
            deviationAngle = -PI;

        // constrain camera's height
        if(heightAngle < -PI/2.f && dy < 0.f)
            heightAngle = -PI/2.f + 0.01f;
        else if(heightAngle > PI/2.f && dy > 0.f)
            heightAngle = PI/2.f - 0.01f;
    }
}

void Camera::updateDistance(float offset)
{
    // constrain offset to [-1;1]
    offset = (offset < 0.f) ? -1.f : (offset > 0.f) ? 1.f : 0.f;

    distanceFromCenter += -offset * zoomSpeed;

    if(distanceFromCenter < minDist && offset > 0.f)
        distanceFromCenter = minDist;
    else if (distanceFromCenter > maxDist && offset < 0.f)
        distanceFromCenter = maxDist;
}

void Camera::translate()
{
    if(isTranslating){
        vec3d translation = translationDirX * translateSpeed * mouseDX + translationDirY * translateSpeed * mouseDY;
        center += translation;

        //update();
    }
}

void Camera::update()
{
    // calculate the eye coordinates with spherical to cartesian coordinates conversion
    pos.x = distanceFromCenter * cos(heightAngle) * sin(deviationAngle);
    pos.y = distanceFromCenter * sin(heightAngle);
    pos.z = -distanceFromCenter * cos(heightAngle) * cos(deviationAngle);

    pos += center;
}

void Camera::calculateTranslationDirs()
{
    vec3d centerToCam = pos - center;
    centerToCam.normalize();

    vec3d up = vec3d(0, 1, 0);

    translationDirY = up;
    translationDirY.normalize();

    translationDirX = vec3d::cross(centerToCam, up);
    translationDirX.normalize();
}

void Camera::enableRotation()
{
    if(!isTranslating)
        isRotating = true;
}

void Camera::disableRotation()
{
    isRotating = false;
}

void Camera::enableTranslation()
{
    if(!isRotating){
        isTranslating = true;
        calculateTranslationDirs();
    }
}

void Camera::disableTranslation()
{
    isTranslating = false;
}

void Camera::resetCenter()
{
    center = vec3d();
}

Camera::~Camera()
{

}
