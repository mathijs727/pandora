#include "fps_camera_controls.h"
#include <algorithm>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

const double PI = 3.141592653589793;

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
                glm::vec3 left = m_orientation * glm::vec3(-1, 0, 0);
                m_orientation = glm::angleAxis((float)pitchDelta, left) * m_orientation;
                m_orientation = glm::angleAxis((float)yawDelta, m_up) * m_orientation;

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
    float moveSpeed = 0.01f;
    glm::vec3 forward = m_orientation * glm::vec3(0.0f, 0.0f, 1.0f * m_scale.z);
    glm::vec3 left = m_orientation * glm::vec3(1.0f * m_scale.x, 0.0f, 0.0f);

    if (m_window.isKeyDown(GLFW_KEY_A)) {
        m_position += -left * deltaMsFloat * moveSpeed;
        m_cameraChanged = true;
    }
    if (m_window.isKeyDown(GLFW_KEY_D)) {
        m_position += left * deltaMsFloat * moveSpeed;
        m_cameraChanged = true;
    }
    if (m_window.isKeyDown(GLFW_KEY_W)) {
        m_position += forward * deltaMsFloat * moveSpeed;
        m_cameraChanged = true;
    }
    if (m_window.isKeyDown(GLFW_KEY_S)) {
        m_position += -forward * deltaMsFloat * moveSpeed;
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
