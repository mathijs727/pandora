#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/traversal/acceleration_structure.h"
#include "stream/task_graph.h"
#include <atomic>
#include <glm/vec2.hpp>
#include <tuple>

namespace pandora {

class NormalDebugIntegrator {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    NormalDebugIntegrator(
        tasking::TaskGraph* pTaskGraph);

    struct RayState {
        glm::ivec2 pixel;
    };
    struct AnyRayState {
        glm::ivec2 pixel;
    };
    using HitTaskHandle = tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, RayState>>;
    using MissTaskHandle = tasking::TaskHandle<std::tuple<Ray, RayState>>;
    using AnyHitTaskHandle = tasking::TaskHandle<std::tuple<Ray, AnyRayState>>;
    using AnyMissTaskHandle = tasking::TaskHandle<std::tuple<Ray, AnyRayState>>;

    HitTaskHandle hitTaskHandle() const;
    MissTaskHandle missTaskHandle() const;
    AnyHitTaskHandle anyHitTaskHandle() const;
    AnyMissTaskHandle anyMissTaskHandle() const;

    using Accel = AccelerationStructure<RayState, AnyRayState>;
    void render(const PerspectiveCamera& camera, Sensor& sensor, const Scene& scene, const Accel& accel, size_t seed = 0);

private:
    void rayHit(const Ray& ray, const SurfaceInteraction& si, const RayState& state);
    void rayMiss(const Ray& ray, const RayState& state);
    void rayAnyHit(const Ray& ray, const AnyRayState& state);
    void rayAnyMiss(const Ray& ray, const AnyRayState& state);

    void spawnNewPaths(int numPaths);

private:
    tasking::TaskGraph* m_pTaskGraph;

    HitTaskHandle m_hitTask;
    MissTaskHandle m_missTask;
    AnyHitTaskHandle m_anyHitTask;
    AnyMissTaskHandle m_anyMissTask;

    // TODO: make render state local to render() instead of spreading it around the class
    const PerspectiveCamera* m_pCamera;
    Sensor* m_pSensor;
    std::atomic_int m_currentRayIndex;
    glm::ivec2 m_resolution;
    glm::vec2 m_fResolution;
    int m_maxPixelIndex;

    const Accel* m_pAccelerationStructure;
};

}
