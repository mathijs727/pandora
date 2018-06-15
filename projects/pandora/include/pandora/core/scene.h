#pragma once
#include "glm/glm.hpp"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/area_light.h"
#include "pandora/shading/material.h"
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

    gsl::span<const SceneObject> getSceneObjects() const;

private:
    std::vector<SceneObject> m_sceneObject;
};

}
