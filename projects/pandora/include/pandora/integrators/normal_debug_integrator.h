#pragma once
#include "pandora/core/pandora.h"
#include "pandora/core/integrator.h"

namespace pandora {

struct NormalDebugIntegratorState
{
    glm::vec2 pixel;
};

class NormalDebugIntegrator : public Integrator<NormalDebugIntegratorState>
{
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    NormalDebugIntegrator(const Scene& scene, Sensor& sensor);

    void reset() override final;
    void render(const PerspectiveCamera& camera) override final;

private:
    void rayHit(const Ray& r, SurfaceInteraction si, const NormalDebugIntegratorState& s) override final;
    void rayAnyHit(const Ray& r, const NormalDebugIntegratorState& s) override final;
    void rayMiss(const Ray& r, const NormalDebugIntegratorState& s) override final;

private:
    const glm::ivec2 m_resolution;
};

}
