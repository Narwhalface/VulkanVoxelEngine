#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>

class Camera {
public:
    Camera();

    void setPerspective(float fieldOfViewDegrees, float aspectRatioValue, float nearPlaneValue, float farPlaneValue);
    void setPosition(const glm::vec3& value);
    void setUp(const glm::vec3& value);
    void setDirection(const glm::vec3& value);
    void lookAt(const glm::vec3& target, const glm::vec3& upHint = glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 position() const;
    glm::vec3 direction() const;
    glm::vec3 upVector() const;

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix() const;
    glm::mat4 viewProjectionMatrix() const;

private:
    glm::vec3 eyePosition;
    glm::vec3 forwardDirection;
    glm::vec3 upDirection;
    float fieldOfViewRadians;
    float aspectRatio;
    float nearPlane;
    float farPlane;
};

#endif // CAMERA_HPP
