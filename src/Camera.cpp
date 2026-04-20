#include "Camera.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kEpsilon = 1e-6f;
}

/**
 * Sets up default camera position, orientation, and projection values.
 * @return No return value.
 */
Camera::Camera()
	: eyePosition(0.0f, 0.0f, 0.0f),
	  forwardDirection(0.0f, 0.0f, -1.0f),
	  upDirection(0.0f, 1.0f, 0.0f),
	  fieldOfViewRadians(glm::radians(45.0f)),
	  aspectRatio(4.0f / 3.0f),
	  nearPlane(0.1f),
	  farPlane(500.0f) {}

/**
 * Updates perspective projection settings.
 * @param fieldOfViewDegrees Vertical field of view in degrees.
 * @param aspectRatioValue Width-to-height aspect ratio.
 * @param nearPlaneValue Near clipping distance.
 * @param farPlaneValue Far clipping distance.
 * @return No return value.
 */
void Camera::setPerspective(float fieldOfViewDegrees, float aspectRatioValue, float nearPlaneValue, float farPlaneValue) {
	fieldOfViewRadians = glm::radians(fieldOfViewDegrees);
	aspectRatio = aspectRatioValue;
	nearPlane = std::max(nearPlaneValue, kEpsilon);
	farPlane = std::max(farPlaneValue, nearPlane + kEpsilon);
}

/**
 * Sets camera world-space position.
 * @param value New position vector.
 * @return No return value.
 */
void Camera::setPosition(const glm::vec3& value) {
	eyePosition = value;
}

/**
 * Sets camera up direction.
 * @param value New up vector.
 * @return No return value.
 */
void Camera::setUp(const glm::vec3& value) {
	if (glm::dot(value, value) <= kEpsilon) {
		return;
	}
	upDirection = glm::normalize(value);
}

/**
 * Sets camera forward direction.
 * @param value New forward vector.
 * @return No return value.
 */
void Camera::setDirection(const glm::vec3& value) {
	if (glm::dot(value, value) <= kEpsilon) {
		return;
	}
	forwardDirection = glm::normalize(value);
}

/**
 * Rotates camera to look at a world-space target.
 * @param target Target point to look toward.
 * @param upHint Optional up direction hint.
 * @return No return value.
 */
void Camera::lookAt(const glm::vec3& target, const glm::vec3& upHint) {
	setUp(upHint);
	setDirection(target - eyePosition);
}

/**
 * Returns current camera position.
 * @return Camera position vector.
 */
glm::vec3 Camera::position() const {
	return eyePosition;
}

/**
 * Returns current camera forward direction.
 * @return Normalized forward direction vector.
 */
glm::vec3 Camera::direction() const {
	return forwardDirection;
}

/**
 * Returns current camera up direction.
 * @return Normalized up direction vector.
 */
glm::vec3 Camera::upVector() const {
	return upDirection;
}

/**
 * Builds the view matrix from camera state.
 * @return View matrix.
 */
glm::mat4 Camera::viewMatrix() const {
	return glm::lookAt(eyePosition, eyePosition + forwardDirection, upDirection);
}

/**
 * Builds the projection matrix from camera perspective settings.
 * @return Projection matrix.
 */
glm::mat4 Camera::projectionMatrix() const {
	glm::mat4 projection = glm::perspective(fieldOfViewRadians, aspectRatio, nearPlane, farPlane);
	projection[1][1] *= -1.0f;
	return projection;
}

/**
 * Builds the combined projection * view matrix.
 * @return View-projection matrix.
 */
glm::mat4 Camera::viewProjectionMatrix() const {
	return projectionMatrix() * viewMatrix();
}
