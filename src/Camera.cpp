#include "Camera.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kEpsilon = 1e-6f;
}

Camera::Camera()
	// Initializes default camera position/orientation and projection values; takes no inputs and returns a Camera instance.
	: eyePosition(0.0f, 0.0f, 0.0f),
	  forwardDirection(0.0f, 0.0f, -1.0f),
	  upDirection(0.0f, 1.0f, 0.0f),
	  fieldOfViewRadians(glm::radians(45.0f)),
	  aspectRatio(4.0f / 3.0f),
	  nearPlane(0.1f),
	  farPlane(500.0f) {}

void Camera::setPerspective(float fieldOfViewDegrees, float aspectRatioValue, float nearPlaneValue, float farPlaneValue) {
	// Sets perspective parameters from FOV/aspect/near/far inputs and clamps valid planes; outputs no return value.
	fieldOfViewRadians = glm::radians(fieldOfViewDegrees);
	aspectRatio = aspectRatioValue;
	nearPlane = std::max(nearPlaneValue, kEpsilon);
	farPlane = std::max(farPlaneValue, nearPlane + kEpsilon);
}

void Camera::setPosition(const glm::vec3& value) {
	// Sets camera world position from value input; outputs no return value.
	eyePosition = value;
}

void Camera::setUp(const glm::vec3& value) {
	// Sets normalized up vector when input is non-zero; outputs no return value.
	if (glm::dot(value, value) <= kEpsilon) {
		return;
	}
	upDirection = glm::normalize(value);
}

void Camera::setDirection(const glm::vec3& value) {
	// Sets normalized forward direction when input is non-zero; outputs no return value.
	if (glm::dot(value, value) <= kEpsilon) {
		return;
	}
	forwardDirection = glm::normalize(value);
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& upHint) {
	// Orients camera toward target using an optional up hint input; outputs no return value.
	setUp(upHint);
	setDirection(target - eyePosition);
}

glm::vec3 Camera::position() const {
	// Returns current camera position; takes no inputs.
	return eyePosition;
}

glm::vec3 Camera::direction() const {
	// Returns current forward direction vector; takes no inputs.
	return forwardDirection;
}

glm::vec3 Camera::upVector() const {
	// Returns current up vector; takes no inputs.
	return upDirection;
}

glm::mat4 Camera::viewMatrix() const {
	// Builds and returns the view matrix from position and orientation; takes no inputs.
	return glm::lookAt(eyePosition, eyePosition + forwardDirection, upDirection);
}

glm::mat4 Camera::projectionMatrix() const {
	// Builds and returns a Vulkan-corrected perspective projection matrix; takes no inputs.
	glm::mat4 projection = glm::perspective(fieldOfViewRadians, aspectRatio, nearPlane, farPlane);
	projection[1][1] *= -1.0f;
	return projection;
}

glm::mat4 Camera::viewProjectionMatrix() const {
	// Returns combined projection*view matrix for rendering; takes no inputs.
	return projectionMatrix() * viewMatrix();
}
