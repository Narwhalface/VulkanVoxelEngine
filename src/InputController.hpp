#ifndef INPUT_CONTROLLER_HPP
#define INPUT_CONTROLLER_HPP

#include <glfw3.h>

#include <glm/glm.hpp>

#include "Camera.hpp"

class InputController {
public:
    /**
     * Initializes input controller state.
     * @return No return value.
     */
    InputController();

    /**
     * Attaches the input controller to a window and camera.
     * @param windowHandle GLFW window used for input polling.
     * @param cameraRef Camera updated by controller input.
     * @return No return value.
     */
    void attach(GLFWwindow* windowHandle, Camera* cameraRef);
    /**
     * Processes one frame of input and applies camera changes.
     * @param deltaTime Frame delta time in seconds.
     * @return No return value.
     */
    void update(double deltaTime);
    /**
     * Aligns internal yaw/pitch values with current camera orientation.
     * @return No return value.
     */
    void syncOrientationFromCamera();
    /**
     * Sets base movement speed.
     * @param speed Movement speed units per second.
     * @return No return value.
     */
    void setMovementSpeed(float speed);
    /**
     * Sets sprint speed multiplier.
     * @param multiplier Sprint multiplier applied while sprint key is held.
     * @return No return value.
     */
    void setSprintMultiplier(float multiplier);
    /**
     * Sets mouse look sensitivity.
     * @param sensitivity Mouse sensitivity scalar.
     * @return No return value.
     */
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

    /**
     * Updates yaw/pitch using mouse delta offsets.
     * @param xOffset Horizontal mouse delta.
     * @param yOffset Vertical mouse delta.
     * @return No return value.
     */
    void updateOrientation(double xOffset, double yOffset);
    /**
     * Updates camera position from movement input.
     * @param deltaTimeSeconds Frame delta time in seconds.
     * @param forward Forward basis vector.
     * @param right Right basis vector.
     * @param worldUp Global up vector.
     * @return No return value.
     */
    void updatePosition(double deltaTimeSeconds, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& worldUp);
};

#endif // INPUT_CONTROLLER_HPP
