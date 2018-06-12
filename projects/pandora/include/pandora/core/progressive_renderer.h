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
    void incrementalRender(PerspectiveCamera& camera);

    int getSampleCount() const;

private:
    int m_resolutionX, m_resolutionY;
    int m_spp;

    const Scene& m_scene;
};

}
