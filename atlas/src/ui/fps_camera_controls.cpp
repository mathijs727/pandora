#include "fps_camera_controls.h"
#include "pandora/math/constants.h"
#include "pandora/math/quaternion.h"
#include <algorithm>
#include <iostream>

namespace atlas {

FpsCameraControls::FpsCameraControls(Window& window, PerspectiveCamera& camera)
    : m_window(window)
    , m_camera(camera)
    , m_pitch(0.0)
    , m_yaw(0.0)
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
            Vec2d delta2D = position - m_previousMousePos;
            m_pitch += static_cast<float>(delta2D.y * lookSpeed);
            m_yaw += static_cast<float>(delta2D.x * lookSpeed);

            // Limit pitch movement so we cant make "loopings"
            m_pitch = std::clamp(m_pitch, -piD / 2.0 + 0.1, piD / 2.0 - 0.1);

            QuatF pitchRotation = QuatF::rotation(Vec3f(1.0f, 0.0f, 0.0f), m_pitch);
            QuatF yawRotation = QuatF::rotation(Vec3f(0.0f, 1.0f, 0.0f), m_yaw);
            QuatF cameraOrientation = pitchRotation * yawRotation;

            Vec3f forward = cameraOrientation.rotateVector(Vec3f(0.0f, 0.0f, 1.0f));
            Vec3f up = cameraOrientation.rotateVector(Vec3f(0.0f, 1.0f, 0.0f));

            m_camera.setOrientation(forward, up);
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

bool FpsCameraControls::cameraChanged()
{
    bool changed = m_cameraChanged;
    m_cameraChanged = false;
    return changed;
}
}