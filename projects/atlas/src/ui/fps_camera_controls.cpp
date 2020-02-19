#include "fps_camera_controls.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace atlas {

FpsCameraControls::FpsCameraControls(Window& window, PerspectiveCamera& camera)
    : m_window(window)
    , m_camera(camera)
    //, m_cameraEulerAngles(0.0)
    //, m_position(camera.getTransform() * glm::vec4(0, 0, 0, 1))
    //, m_orientation(glm::quat_cast(camera.getTransform()))
    , m_previousFrameTimePoint(clock::now())
    , m_initialFrame(true)
    , m_cameraChanged(true)
    , m_mouseCaptured(false)
    , m_previousMousePos(0.0)
{
    // For some reason glm::quat_cast gives the wrong results and glm::decompose gives the correct results
    // but with a possible scale of (-1, -1, -1). So keep track of the scale, its ugly but it works.
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(camera.getTransform(), m_scale, m_orientation, m_position, skew, perspective);
    m_up = m_orientation * glm::vec3(0, 1, 0);

    double lookSpeed = 0.0002;
    m_window.registerMouseMoveCallback([this, lookSpeed](glm::dvec2 position) {
        if (!m_mouseCaptured)
            return;

        if (!m_initialFrame) {
            // Moving mouse to the left (negative x) should result in a counter-clock wise rotation so we multiply
            // the yaw rotation by minus one.
            glm::dvec2 delta2D = position - m_previousMousePos;
            double pitchDelta = delta2D.y * lookSpeed;
            double yawDelta = delta2D.x * lookSpeed;

            if (pitchDelta != 0.0 || yawDelta != 0.0) {
                const glm::vec3 left = m_orientation * glm::vec3(-1, 0, 0);
                m_orientation = glm::angleAxis((float)pitchDelta, left) * m_orientation;
                m_orientation = glm::angleAxis((float)yawDelta, left) * m_orientation;

                m_cameraChanged = true;
            }

            if (m_cameraChanged) {
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_position);
                transform *= glm::mat4_cast(m_orientation);
                transform = glm::scale(transform, m_scale);
                m_camera.setTransform(transform);
            }
        } else {
            m_initialFrame = false;
        }

        m_previousMousePos = position;
    });

    m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
            m_mouseCaptured = !m_mouseCaptured;
            m_initialFrame = true; // Switching between capture modes changes the cursor position coordinate system
            m_window.setMouseCapture(m_mouseCaptured);
        }

        if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_PRESS)
            m_moveSpeed *= 0.8f;
        if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_PRESS)
            m_moveSpeed *= 1.2f;
    });
}

void FpsCameraControls::tick()
{
    // Move camera
    auto now = clock::now();
    auto delta = now - m_previousFrameTimePoint;
    auto deltaMs = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
    float deltaMsFloat = deltaMs.count() / 10000000.0f;

    //TODO(Mathijs): store forward, left and up vectors in the class itself
    glm::vec3 forward = m_orientation * glm::vec3(0.0f, 0.0f, 1.0f * m_scale.z);
    glm::vec3 left = m_orientation * glm::vec3(1.0f * m_scale.x, 0.0f, 0.0f);

    if (m_window.isKeyDown(GLFW_KEY_A)) {
        m_position += -left * deltaMsFloat * m_moveSpeed;
        m_cameraChanged = true;
    }
    if (m_window.isKeyDown(GLFW_KEY_D)) {
        m_position += left * deltaMsFloat * m_moveSpeed;
        m_cameraChanged = true;
    }
    if (m_window.isKeyDown(GLFW_KEY_W)) {
        m_position += forward * deltaMsFloat * m_moveSpeed;
        m_cameraChanged = true;
    }
    if (m_window.isKeyDown(GLFW_KEY_S)) {
        m_position += -forward * deltaMsFloat * m_moveSpeed;
        m_cameraChanged = true;
    }

    if (m_cameraChanged) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_position);
        transform *= glm::mat4_cast(m_orientation);
        transform = glm::scale(transform, m_scale);
        m_camera.setTransform(transform);
    }

    m_previousFrameTimePoint = now;
}

bool FpsCameraControls::cameraChanged()
{
    bool changed = m_cameraChanged;
    m_cameraChanged = false;
    return changed;
}
}
