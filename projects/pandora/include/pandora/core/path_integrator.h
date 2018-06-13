#pragma once
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/core/sensor.h"
#include "pandora/traversal/embree_accel.h"
#include <tbb/concurrent_vector.h>
#include <variant>

namespace pandora {

class PathIntegrator {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor);

    void render(const PerspectiveCamera& camera);

private:
    struct NewRays {
        glm::vec3 continuationRayWeight;
        Ray continuationRay;
    };

    std::variant<NewRays, glm::vec3> performShading(glm::vec3 weight, const Ray& ray, const IntersectionData& intersection) const;

private:
    const int m_maxDepth;
    Sensor& m_sensor;
    const Scene& m_scene;

    struct PathState {
        glm::ivec2 pixel;
        glm::vec3 weight;
        int depth;
    };
    EmbreeAccel<PathState> m_accelerationStructure;
};

}
