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

// Constructs input controller with default sensitivity/speed state; takes no inputs and returns an InputController instance.
InputController::InputController() = default;

void InputController::attach(GLFWwindow* windowHandle, Camera* cameraRef) {
    // Attaches GLFW window and camera pointers used for input updates; outputs no return value.
    window = windowHandle;
    camera = cameraRef;
    firstMouseSample = true;

    if (window != nullptr) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    syncOrientationFromCamera();
}

void InputController::setMovementSpeed(float speed) {
    // Sets movement speed from speed input clamped to non-negative; outputs no return value.
    movementSpeed = std::max(speed, 0.0f);
}

void InputController::setSprintMultiplier(float multiplier) {
    // Sets sprint multiplier from multiplier input clamped to at least 1; outputs no return value.
    sprintMultiplier = std::max(multiplier, 1.0f);
}

void InputController::setMouseSensitivity(float sensitivity) {
    // Sets mouse sensitivity from sensitivity input clamped to non-negative; outputs no return value.
    mouseSensitivity = std::max(sensitivity, 0.0f);
}

void InputController::syncOrientationFromCamera() {
    // Synchronizes yaw/pitch from current camera direction state; outputs no return value.
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

void InputController::update(double deltaTime) {
    // Processes input for one frame using deltaTime seconds and applies camera movement/rotation; outputs no return value.
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

void InputController::updateOrientation(double xOffset, double yOffset) {
    // Updates yaw/pitch from mouse deltas xOffset/yOffset; outputs no return value.
    const float scaledX = static_cast<float>(xOffset) * mouseSensitivity;
    const float scaledY = static_cast<float>(yOffset) * mouseSensitivity;

    yawDegrees += scaledX;
    pitchDegrees += scaledY;
    pitchDegrees = std::clamp(pitchDegrees, kMinPitch, kMaxPitch);
}

void InputController::updatePosition(double deltaTimeSeconds, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& worldUp) {
    // Applies keyboard movement using deltaTime and basis vectors forward/right/worldUp; outputs no return value.
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
