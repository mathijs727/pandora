#pragma once
#include "pandora/core/perspective_camera.h"
#include "ui/window.h"
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
    glm::vec3 m_up;

    glm::vec3 m_scale;
    glm::quat m_orientation;
    glm::vec3 m_position;

    using clock = std::chrono::high_resolution_clock;
    clock::time_point m_previousFrameTimePoint;
    bool m_initialFrame;
    bool m_cameraChanged;
    bool m_mouseCaptured;
    glm::dvec2 m_previousMousePos;
};
}
