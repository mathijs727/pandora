#pragma once
#include "pandora/integrators/sampler_integrator.h"
#include "pandora/svo/sparse_voxel_octree.h"

namespace pandora {

class SVODepthTestIntegrator : public SamplerIntegrator {
public:
    SVODepthTestIntegrator(const Scene& scene, Sensor& sensor, int spp, int parallelSamples = 1);

private:
    void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s) override final;
    void rayAnyHit(const Ray& r, const RayState& s) override final {};
    void rayMiss(const Ray& r, const RayState& s) override final;

    static SparseVoxelOctree buildSVO(const Scene& scene);

private:
    SparseVoxelOctree m_svo;

    glm::mat4 m_worldToSVO;
};

}
