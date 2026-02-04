#ifndef INPUT_CONTROLLER_HPP
#define INPUT_CONTROLLER_HPP

#include <glfw3.h>

#include <glm/glm.hpp>

#include "Camera.hpp"

class InputController {
public:
    InputController();

    void attach(GLFWwindow* windowHandle, Camera* cameraRef);
    void update(double deltaTime);
    void syncOrientationFromCamera();
    void setMovementSpeed(float speed);
    void setSprintMultiplier(float multiplier);
    void setMouseSensitivity(float sensitivity);

private:
    GLFWwindow* window = nullptr;
    Camera* camera = nullptr;
    float yawDegrees = -90.0f;
    float pitchDegrees = 0.0f;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;
    bool firstMouseSample = true;
    float movementSpeed = 15.0f;
    float sprintMultiplier = 3.0f;
    float mouseSensitivity = 0.1f;

    void updateOrientation(double xOffset, double yOffset);
    void updatePosition(double deltaTimeSeconds, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& worldUp);
};

#endif // INPUT_CONTROLLER_HPP
