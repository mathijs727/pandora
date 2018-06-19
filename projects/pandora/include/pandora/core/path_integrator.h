#pragma once
#include "pandora/core/integrator.h"
#include "pandora/core/pandora.h"
#include <variant>

namespace pandora {

struct PathIntegratorState {
    glm::ivec2 pixel;
    glm::vec3 weight;
    int depth;
};

class PathIntegrator : Integrator<PathIntegratorState> {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor);

    void render(const PerspectiveCamera& camera) final;

protected:
    void rayHit(const Ray& r, const SurfaceInteraction& si, const PathIntegratorState& s, const EmbreeInsertHandle& h) final;
    void rayMiss(const Ray& r, const PathIntegratorState& s) final;

private:
    const int m_maxDepth;
};

}
