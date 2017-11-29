#pragma once
#include "pandora/core/perspective_camera.h"
#include "ui/window.h"

using namespace pandora;

namespace atlas {

class FpsCameraControls {
public:
    FpsCameraControls(Window& window, PerspectiveCamera& camera);

    bool cameraChanged();

private:
    Window& m_window;

    PerspectiveCamera& m_camera;
    double m_pitch, m_yaw;

    bool m_initialFrame;
    bool m_cameraChanged;
    bool m_mouseCaptured;
    Vec2d m_previousMousePos;
};
}