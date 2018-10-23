#pragma once
#include "pandora/integrators/sampler_integrator.h"

namespace pandora {

class NaiveDirectLightingIntegrator : public SamplerIntegrator {
public:
    NaiveDirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, int parallelSamples = 1);

private:
    void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h) override final;
    void rayAnyHit(const Ray& r, const RayState& s) override final {};
    void rayMiss(const Ray& r, const RayState& s) override final;
};

}
