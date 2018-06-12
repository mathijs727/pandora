#pragma once
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/core/sensor.h"
#include <tbb/concurrent_vector.h>
#include <variant>

namespace pandora {

class PathIntegrator {
public:
    PathIntegrator(int maxDepth, PerspectiveCamera& camera);

    void render(const Scene& scene);

private:
    struct Continuation {
        Ray continuationRay;
        //Ray shadowRay;
    };
    std::variant<Continuation, glm::vec3> performShading(const Scene& scene, const Ray& ray, const IntersectionData& intersectionData);

private:
    PerspectiveCamera& m_camera;
    const int m_maxDepth;

    tbb::concurrent_vector<Ray> m_rayQueue1;
    tbb::concurrent_vector<Ray> m_rayQueue2;
};

}
