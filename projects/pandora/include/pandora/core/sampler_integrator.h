#pragma once
#include "pandora/core/pandora.h"
#include "pandora/samplers/uniform_sampler.h"
#include "pandora/traversal/embree_accel.h"
#include <vector>

namespace pandora
{

class SamplerIntegrator {
public:
    SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor);

    void render(const PerspectiveCamera& camera);
private:
    void handleSurfaceInteraction();

    Sampler& getSampler(const glm::ivec2& pixel);
private:
    const int m_maxDepth;
    Sensor& m_sensor;
    const Scene& m_scene;

    std::vector<UniformSampler> m_samplers;

    struct PathState {
        glm::ivec2 pixel;
        glm::vec3 weight;
        int depth;
    };
    EmbreeAccel<PathState> m_accelerationStructure;
};

}
