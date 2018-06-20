#pragma once
#include "glm/glm.hpp"
#include "pandora/core/material.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/area_light.h"
#include <gsl/span>
#include <memory>
#include <tuple>
#include <vector>

namespace pandora {

struct SceneObject {
    std::shared_ptr<const TriangleMesh> mesh;
    std::shared_ptr<const Material> material;
    std::unique_ptr<const AreaLight[]> areaLightPerPrimitive;
};

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    void addSceneObject(SceneObject&& sceneNode);

    void addInfiniteLight(const std::shared_ptr<Light>& light);

    gsl::span<const SceneObject> getSceneObjects() const;
    gsl::span<const std::shared_ptr<Light>> getLights() const;
    gsl::span<const std::shared_ptr<Light>> getInfiniteLIghts() const;

private:
    std::vector<SceneObject> m_sceneObject;
    std::vector<std::shared_ptr<Light>> m_lights;
    std::vector<std::shared_ptr<Light>> m_infiniteLights;
};

}
