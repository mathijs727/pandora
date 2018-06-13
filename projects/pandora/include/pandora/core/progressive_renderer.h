#pragma once
#include "pandora/core/path_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sensor.h"

#include <memory>

namespace pandora {

class ProgressiveRenderer {
public:
    ProgressiveRenderer(const Scene& scene, Sensor& sensor);

    void clear();
    void incrementalRender(const PerspectiveCamera& camera);

    int getSampleCount() const;

private:
    int m_spp;

    Sensor& m_sensor;
    PathIntegrator m_integrator;
};

}
