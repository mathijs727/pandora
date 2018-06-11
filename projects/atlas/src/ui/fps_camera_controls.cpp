#include "fps_camera_controls.h"
#include "glm/gtc/quaternion.hpp"
#include <algorithm>
#include <iostream>

const double PI = 3.141592653589793;

namespace atlas {

FpsCameraControls::FpsCameraControls(Window& window, PerspectiveCamera& camera)
    : m_window(window)
    , m_camera(camera)
    , m_cameraEulerAngles(0.0)
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
            glm::quat currentOrienation = m_camera.getOrienation();

            // Moving mouse to the left (negative x) should result in a counter-clock wise rotation so we multiply
            // the yaw rotation by minus one.
            glm::dvec2 delta2D = position - m_previousMousePos;
            double pitchDelta = delta2D.y * lookSpeed;
            double yawDelta = -(delta2D.x * lookSpeed);

            if (pitchDelta != 0.0 || yawDelta != 0.0) {
                m_cameraEulerAngles.x += pitchDelta;
                m_cameraEulerAngles.x = std::clamp(m_cameraEulerAngles.x, -PI / 2.0, PI / 2.0);
                m_cameraEulerAngles.y += yawDelta;

                auto quat = glm::quat(m_cameraEulerAngles);
                m_camera.setOrientation(quat);

                glm::vec3 forward = quat * glm::vec3(0, 0, 1);
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
    glm::vec3 cameraPosition = m_camera.getPosition();
    glm::quat cameraOrientation = m_camera.getOrienation();

    glm::vec3 forward = cameraOrientation * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 left = cameraOrientation * glm::vec3(1.0f, 0.0f, 0.0f);

    if (m_window.isKeyDown(GLFW_KEY_A))
        cameraPosition += left * deltaMsFloat * moveSpeed;
    if (m_window.isKeyDown(GLFW_KEY_D))
        cameraPosition += -left * deltaMsFloat * moveSpeed;
    if (m_window.isKeyDown(GLFW_KEY_W))
        cameraPosition += forward * deltaMsFloat * moveSpeed;
    if (m_window.isKeyDown(GLFW_KEY_S))
        cameraPosition += -forward * deltaMsFloat * moveSpeed;

    m_camera.setPosition(cameraPosition);

    m_previousFrameTimePoint = now;
}

bool FpsCameraControls::cameraChanged()
{
    bool changed = m_cameraChanged;
    m_cameraChanged = false;
    return changed;
}
}
