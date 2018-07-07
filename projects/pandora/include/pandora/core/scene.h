#pragma once
#include "glm/glm.hpp"
#include "pandora/core/material.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/area_light.h"
#include <gsl/span>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace pandora {

class SceneObject {
public:
    SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material);
    SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);

    // TODO: intersect function (with custom geometry in Embree)?
    inline const TriangleMesh& getMesh() const { return *m_mesh; };
    inline const Material& getMaterial() const { return *m_material; };

    const AreaLight* getAreaLight(unsigned primID) const;
    std::optional<gsl::span<const AreaLight>> getAreaLights() const;

private:
    std::shared_ptr<const TriangleMesh> m_mesh;
    std::shared_ptr<const Material> m_material;
    std::vector<AreaLight> m_areaLightPerPrimitive;
};

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    void addSceneObject(std::unique_ptr<SceneObject>&& sceneNode);

    void addInfiniteLight(const std::shared_ptr<Light>& light);

    gsl::span<const std::unique_ptr<SceneObject>> getSceneObjects() const;
    gsl::span<const Light* const> getLights() const;
    gsl::span<const Light* const> getInfiniteLights() const;

private:
    std::vector<std::unique_ptr<SceneObject>> m_sceneObject;
    std::vector<const Light*> m_lights;
    std::vector<const Light*> m_infiniteLights;

    std::vector<std::shared_ptr<Light>> m_lightOwningPointers;
};

}
