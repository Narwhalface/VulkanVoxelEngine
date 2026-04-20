#include "InputController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kMaxPitch = 89.0f;
constexpr float kMinPitch = -89.0f;
const glm::vec3 kWorldUp(0.0f, 1.0f, 0.0f);
}

/**
 * Initializes input controller state.
 * @return No return value.
 */
InputController::InputController() = default;

/**
 * Attaches the input controller to a window and camera.
 * @param windowHandle GLFW window used for input polling.
 * @param cameraRef Camera updated by controller input.
 * @return No return value.
 */
void InputController::attach(GLFWwindow* windowHandle, Camera* cameraRef) {
    window = windowHandle;
    camera = cameraRef;
    firstMouseSample = true;

    if (window != nullptr) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    syncOrientationFromCamera();
}

/**
 * Sets base movement speed.
 * @param speed Movement speed units per second.
 * @return No return value.
 */
void InputController::setMovementSpeed(float speed) {
    movementSpeed = std::max(speed, 0.0f);
}

/**
 * Sets sprint speed multiplier.
 * @param multiplier Sprint multiplier applied while sprint key is held.
 * @return No return value.
 */
void InputController::setSprintMultiplier(float multiplier) {
    sprintMultiplier = std::max(multiplier, 1.0f);
}

/**
 * Sets mouse look sensitivity.
 * @param sensitivity Mouse sensitivity scalar.
 * @return No return value.
 */
void InputController::setMouseSensitivity(float sensitivity) {
    mouseSensitivity = std::max(sensitivity, 0.0f);
}

/**
 * Aligns internal yaw/pitch values with current camera orientation.
 * @return No return value.
 */
void InputController::syncOrientationFromCamera() {
    if (camera == nullptr) {
        return;
    }

    const glm::vec3 forward = glm::normalize(camera->direction());
    if (std::isnan(forward.x) || std::isnan(forward.y) || std::isnan(forward.z)) {
        return;
    }

    yawDegrees = glm::degrees(std::atan2(forward.z, forward.x));
    pitchDegrees = glm::degrees(std::asin(std::clamp(forward.y, -1.0f, 1.0f)));
    pitchDegrees = std::clamp(pitchDegrees, kMinPitch, kMaxPitch);
}

/**
 * Processes one frame of input and applies camera changes.
 * @param deltaTime Frame delta time in seconds.
 * @return No return value.
 */
void InputController::update(double deltaTime) {
    if (window == nullptr || camera == nullptr) {
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (firstMouseSample) {
        lastCursorX = cursorX;
        lastCursorY = cursorY;
        firstMouseSample = false;
    }

    const double xOffset = cursorX - lastCursorX;
    const double yOffset = lastCursorY - cursorY;
    lastCursorX = cursorX;
    lastCursorY = cursorY;

    updateOrientation(xOffset, yOffset);

    const float yawRadians = glm::radians(yawDegrees);
    const float pitchRadians = glm::radians(pitchDegrees);

    glm::vec3 forward;
    forward.x = std::cos(yawRadians) * std::cos(pitchRadians);
    forward.y = std::sin(pitchRadians);
    forward.z = std::sin(yawRadians) * std::cos(pitchRadians);
    forward = glm::normalize(forward);
    camera->setDirection(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, kWorldUp));
    if (glm::dot(right, right) <= std::numeric_limits<float>::epsilon()) {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    camera->setUp(up);

    updatePosition(deltaTime, forward, right, kWorldUp);
}

/**
 * Updates yaw/pitch using mouse delta offsets.
 * @param xOffset Horizontal mouse delta.
 * @param yOffset Vertical mouse delta.
 * @return No return value.
 */
void InputController::updateOrientation(double xOffset, double yOffset) {
    const float scaledX = static_cast<float>(xOffset) * mouseSensitivity;
    const float scaledY = static_cast<float>(yOffset) * mouseSensitivity;

    yawDegrees += scaledX;
    pitchDegrees += scaledY;
    pitchDegrees = std::clamp(pitchDegrees, kMinPitch, kMaxPitch);
}

/**
 * Updates camera position from movement input.
 * @param deltaTimeSeconds Frame delta time in seconds.
 * @param forward Forward basis vector.
 * @param right Right basis vector.
 * @param worldUp Global up vector.
 * @return No return value.
 */
void InputController::updatePosition(double deltaTimeSeconds, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& worldUp) {
    if (deltaTimeSeconds <= 0.0) {
        return;
    }

    glm::vec3 velocity(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        velocity += forward;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        velocity -= forward;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        velocity += right;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        velocity -= right;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        velocity += worldUp;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        velocity -= worldUp;
    }

    if (glm::dot(velocity, velocity) <= std::numeric_limits<float>::epsilon()) {
        return;
    }

    float speed = movementSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        speed *= sprintMultiplier;
    }

    velocity = glm::normalize(velocity);
    const float distance = speed * static_cast<float>(deltaTimeSeconds);
    const glm::vec3 newPosition = camera->position() + velocity * distance;
    camera->setPosition(newPosition);
}
