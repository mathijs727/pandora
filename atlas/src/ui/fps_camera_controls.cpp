#include "fps_camera_controls.h"
#include "pandora/math/constants.h"
#include "pandora/math/quaternion.h"
#include <algorithm>
#include <iostream>

namespace atlas {

FpsCameraControls::FpsCameraControls(Window& window, PerspectiveCamera& camera)
    : m_window(window)
    , m_camera(camera)
    , m_previousFrameTimePoint(clock::now())
    , m_initialFrame(true)
    , m_cameraChanged(true)
    , m_mouseCaptured(false)
    , m_previousMousePos(0.0)
{
    double lookSpeed = 0.0002;
    m_window.registerMouseMoveCallback([this, lookSpeed](Vec2d position) {
        if (!m_mouseCaptured)
            return;

        if (!m_initialFrame) {
            QuatF currentOrienation = m_camera.getOrienation();

            Vec2d delta2D = position - m_previousMousePos;
            float pitchDelta = static_cast<float>(delta2D.y * lookSpeed);
            float yawDelta = static_cast<float>(delta2D.x * lookSpeed);

            // Limit pitch movement so we cant make "loopings"
            //m_pitch = std::clamp(m_pitch, -piD / 2.0 + 0.1, piD / 2.0 - 0.1);

            QuatF pitchRotation = QuatF::rotation(Vec3f(1.0f, 0.0f, 0.0f), pitchDelta);
            QuatF yawRotation = QuatF::rotation(Vec3f(0.0f, 1.0f, 0.0f), yawDelta);
            QuatF orientationChange = pitchRotation * yawRotation;
            m_camera.setOrientation(currentOrienation * orientationChange);
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
    Vec3f cameraPosition = m_camera.getPosition();
    QuatF cameraOrientation = m_camera.getOrienation();

    Vec3f forward = cameraOrientation.rotateVector(Vec3f(0.0f, 0.0f, 1.0f));
    Vec3f right = cameraOrientation.rotateVector(Vec3f(1.0f, 0.0f, 0.0f));

    if (m_window.isKeyDown(GLFW_KEY_A))
        cameraPosition += right * deltaMsFloat * -moveSpeed;
    if (m_window.isKeyDown(GLFW_KEY_D))
        cameraPosition += right * deltaMsFloat * moveSpeed;
    if (m_window.isKeyDown(GLFW_KEY_W))
        cameraPosition += forward * deltaMsFloat * moveSpeed;
    if (m_window.isKeyDown(GLFW_KEY_S))
        cameraPosition += forward * deltaMsFloat * -moveSpeed;

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