#pragma once
#include "glm/gtc/quaternion.hpp"
#include "pandora/core/perspective_camera.h"
#include "ui/window.h"
#include <chrono>

using namespace pandora;

namespace atlas {

class FpsCameraControls {
public:
    FpsCameraControls(Window& window, PerspectiveCamera& camera);

    void tick();

    bool cameraChanged();

private:
    Window& m_window;
    PerspectiveCamera& m_camera;
    glm::dvec3 m_cameraEulerAngles;

    using clock = std::chrono::high_resolution_clock;
    clock::time_point m_previousFrameTimePoint;
    bool m_initialFrame;
    bool m_cameraChanged;
    bool m_mouseCaptured;
    glm::dvec2 m_previousMousePos;
};
}
