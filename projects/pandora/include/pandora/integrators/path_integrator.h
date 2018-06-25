#pragma once
#include "pandora/core/pandora.h"
#include "pandora/integrators/sampler_integrator.h"

namespace pandora {

class PathIntegrator : SamplerIntegrator {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp);

protected:
    void rayHit(const Ray& r, const SurfaceInteraction& si, const RayState& s, const EmbreeInsertHandle& h) override final;
    void rayMiss(const Ray& r, const RayState& s) override final;
};

}
