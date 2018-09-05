#include "fps_camera_controls.h"
#include <algorithm>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

const double PI = 3.141592653589793;

namespace atlas {

FpsCameraControls::FpsCameraControls(Window& window, PerspectiveCamera& camera)
    : m_window(window)
    , m_camera(camera)
    , m_cameraEulerAngles(0.0)
    , m_position(camera.getTransform() * glm::vec4(0, 0, 0, 1))
    , m_orientation()
    , m_previousFrameTimePoint(clock::now())
    , m_initialFrame(true)
    , m_cameraChanged(true)
    , m_mouseCaptured(false)
    , m_previousMousePos(0.0)
{
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
                m_cameraEulerAngles.x += pitchDelta;
                m_cameraEulerAngles.x = std::clamp(m_cameraEulerAngles.x, -PI / 2.0, PI / 2.0);
                m_cameraEulerAngles.y += yawDelta;

                m_orientation = glm::quat(m_cameraEulerAngles);
                m_cameraChanged = true;
            }

            if (m_cameraChanged) {
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_position);
                transform *= glm::mat4_cast(m_orientation);
                m_camera.setTransform(transform);
            }

            // Limit pitch movement so we cant make "loopings"
            //m_pitch = std::clamp(m_pitch, -piD / 2.0 + 0.1, piD / 2.0 - 0.1);

            //glm::quat pitchRotation = glm::quat::rotation(glm::vec3(1.0f, 0.0f, 0.0f), pitchDelta);
            //glm::quat yawRotation = glm::quat::rotation(glm::vec3(0.0f, 1.0f, 0.0f), yawDelta);
            //glm::quat orientationChange = pitchRotation * yawRotation;
            //m_camera.setOrientation(currentOrienation * orientationChange);
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
    glm::vec3 forward = m_orientation * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 left = m_orientation * glm::vec3(1.0f, 0.0f, 0.0f);

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
        m_camera.setTransform(transform);
    }

    m_previousFrameTimePoint = now;

    auto transform = m_camera.getTransform();
    glm::vec3 position = transform * glm::vec4(0, 0, 0, 1);
    std::cout << "Camera pos: [" << position.x << ", " << position.y << ", " << position.z << "]" << std::endl;
}

bool FpsCameraControls::cameraChanged()
{
    bool changed = m_cameraChanged;
    m_cameraChanged = false;
    return changed;
}
}
