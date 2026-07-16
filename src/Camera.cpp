#include "App.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

Camera::Camera() : m_state({}) {}

void Camera::setPosition(float x, float y, float z) {
    m_state.x = x;
    m_state.y = y;
    m_state.z = z;
}

void Camera::setPosition(const glm::vec3& pos) {
    m_state.x = pos.x;
    m_state.y = pos.y;
    m_state.z = pos.z;
}

glm::vec3 Camera::getPosition() const {
    return glm::vec3(m_state.x, m_state.y, m_state.z);
}

void Camera::update(float yawSpeed, float pitchSpeed, double dt) {
    m_state.yaw   += static_cast<float>(yawSpeed)   * static_cast<float>(dt);
    m_state.pitch += static_cast<float>(pitchSpeed) * static_cast<float>(dt);
    constexpr float PITCH_LIMIT = 89.0f;
    if (m_state.pitch >  PITCH_LIMIT) m_state.pitch =  PITCH_LIMIT;
    if (m_state.pitch < -PITCH_LIMIT) m_state.pitch = -PITCH_LIMIT;
    while (m_state.yaw >  180.0f) m_state.yaw -= 360.0f;
    while (m_state.yaw < -180.0f) m_state.yaw += 360.0f;
}

void Camera::applyDelta(float yawDelta, float pitchDelta) {
    m_state.yaw   += yawDelta;
    m_state.pitch += pitchDelta;
    constexpr float PITCH_LIMIT = 89.0f;
    if (m_state.pitch >  PITCH_LIMIT) m_state.pitch =  PITCH_LIMIT;
    if (m_state.pitch < -PITCH_LIMIT) m_state.pitch = -PITCH_LIMIT;
    while (m_state.yaw >  180.0f) m_state.yaw -= 360.0f;
    while (m_state.yaw < -180.0f) m_state.yaw += 360.0f;
}

glm::mat4 Camera::getViewMatrix() const { return computeViewMatrix(m_state); }

glm::mat4 Camera::computeViewMatrix(const CameraState& cam) const {
    float yaw   = glm::radians(cam.yaw);
    float pitch = glm::radians(cam.pitch);
    // Forward convention matches Renderer::computeViewMatrix (rotate (0,0,-1)
    // by +yaw around +Y): X = -sin(yaw), Z = -cos(yaw).
    glm::vec3 forward(
       -glm::sin(yaw) * glm::cos(pitch),
        glm::sin(pitch),
       -glm::cos(yaw) * glm::cos(pitch)
    );
    forward = glm::normalize(forward);
    const glm::vec3 eye(cam.x, cam.y, cam.z);
    return glm::lookAt(eye, eye + forward, glm::vec3(0.0f, 1.0f, 0.0f));
}
