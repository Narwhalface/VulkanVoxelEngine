#include "Camera.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kEpsilon = 1e-6f;
}

Camera::Camera()
	: eyePosition(0.0f, 0.0f, 0.0f),
	  forwardDirection(0.0f, 0.0f, -1.0f),
	  upDirection(0.0f, 1.0f, 0.0f),
	  fieldOfViewRadians(glm::radians(45.0f)),
	  aspectRatio(4.0f / 3.0f),
	  nearPlane(0.1f),
	  farPlane(500.0f) {}

void Camera::setPerspective(float fieldOfViewDegrees, float aspectRatioValue, float nearPlaneValue, float farPlaneValue) {
	fieldOfViewRadians = glm::radians(fieldOfViewDegrees);
	aspectRatio = aspectRatioValue;
	nearPlane = std::max(nearPlaneValue, kEpsilon);
	farPlane = std::max(farPlaneValue, nearPlane + kEpsilon);
}

void Camera::setPosition(const glm::vec3& value) {
	eyePosition = value;
}

void Camera::setUp(const glm::vec3& value) {
	if (glm::dot(value, value) <= kEpsilon) {
		return;
	}
	upDirection = glm::normalize(value);
}

void Camera::setDirection(const glm::vec3& value) {
	if (glm::dot(value, value) <= kEpsilon) {
		return;
	}
	forwardDirection = glm::normalize(value);
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& upHint) {
	setUp(upHint);
	setDirection(target - eyePosition);
}

glm::vec3 Camera::position() const {
	return eyePosition;
}

glm::vec3 Camera::direction() const {
	return forwardDirection;
}

glm::vec3 Camera::upVector() const {
	return upDirection;
}

glm::mat4 Camera::viewMatrix() const {
	return glm::lookAt(eyePosition, eyePosition + forwardDirection, upDirection);
}

glm::mat4 Camera::projectionMatrix() const {
	glm::mat4 projection = glm::perspective(fieldOfViewRadians, aspectRatio, nearPlane, farPlane);
	projection[1][1] *= -1.0f;
	return projection;
}

glm::mat4 Camera::viewProjectionMatrix() const {
	return projectionMatrix() * viewMatrix();
}
