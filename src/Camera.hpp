#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>

class Camera {
public:
    /**
     * Sets up default camera position, orientation, and projection values.
     * @return No return value.
     */
    Camera();

    /**
     * Updates perspective projection settings.
     * @param fieldOfViewDegrees Vertical field of view in degrees.
     * @param aspectRatioValue Width-to-height aspect ratio.
     * @param nearPlaneValue Near clipping distance.
     * @param farPlaneValue Far clipping distance.
     * @return No return value.
     */
    void setPerspective(float fieldOfViewDegrees, float aspectRatioValue, float nearPlaneValue, float farPlaneValue);
    /**
     * Sets camera world-space position.
     * @param value New position vector.
     * @return No return value.
     */
    void setPosition(const glm::vec3& value);
    /**
     * Sets camera up direction.
     * @param value New up vector.
     * @return No return value.
     */
    void setUp(const glm::vec3& value);
    /**
     * Sets camera forward direction.
     * @param value New forward vector.
     * @return No return value.
     */
    void setDirection(const glm::vec3& value);
    /**
     * Rotates camera to look at a world-space target.
     * @param target Target point to look toward.
     * @param upHint Optional up direction hint.
     * @return No return value.
     */
    void lookAt(const glm::vec3& target, const glm::vec3& upHint = glm::vec3(0.0f, 1.0f, 0.0f));

    /**
     * Returns current camera position.
     * @return Camera position vector.
     */
    glm::vec3 position() const;
    /**
     * Returns current camera forward direction.
     * @return Normalized forward direction vector.
     */
    glm::vec3 direction() const;
    /**
     * Returns current camera up direction.
     * @return Normalized up direction vector.
     */
    glm::vec3 upVector() const;

    /**
     * Builds the view matrix from camera state.
     * @return View matrix.
     */
    glm::mat4 viewMatrix() const;
    /**
     * Builds the projection matrix from camera perspective settings.
     * @return Projection matrix.
     */
    glm::mat4 projectionMatrix() const;
    /**
     * Builds the combined projection * view matrix.
     * @return View-projection matrix.
     */
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
