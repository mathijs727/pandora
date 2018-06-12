#pragma once
#include "pandora/core/sensor.h"
#include "pandora/traversal/embree_accel.h"
#include <memory>

namespace pandora {
class Scene;
class PerspectiveCamera;

class ProgressiveRenderer {
public:
    ProgressiveRenderer(int resolutionX, int resolutionY, const Scene& scene);

    void clear();
    void incrementalRender(const PerspectiveCamera& camera);

    const Sensor& getSensor();
    int getSampleCount() const;

private:
    int m_resolutionX, m_resolutionY;
    int m_spp;
    Sensor m_sensor;

    const Scene& m_scene;
    std::unique_ptr<AccelerationStructure> m_accelerationStructure;
};

}
