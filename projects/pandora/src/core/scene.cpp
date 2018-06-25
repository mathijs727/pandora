#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"

namespace pandora {

SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material)
    : m_mesh(mesh)
    , m_material(material)
{
}

SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted) :
    m_mesh(mesh),
    m_material(material)
{
    m_areaLightPerPrimitive.reserve(m_mesh->numTriangles());
    for (unsigned i = 0; i < mesh->numTriangles(); i++) {
        m_areaLightPerPrimitive.emplace_back(lightEmitted, 1, *mesh, i);
    }
}

const TriangleMesh& SceneObject::getMesh() const
{
    return *m_mesh;
}

const Material& SceneObject::getMaterial() const
{
    return *m_material;
}

const AreaLight* SceneObject::getAreaLight(unsigned primID) const
{
    if (m_areaLightPerPrimitive.empty())
        return nullptr;
    else
        return &m_areaLightPerPrimitive[primID];
}

std::optional<gsl::span<const AreaLight>> SceneObject::getAreaLights() const
{
    if (m_areaLightPerPrimitive.empty())
        return {};
    else
        return m_areaLightPerPrimitive;
}

void Scene::addSceneObject(std::unique_ptr<SceneObject>&& sceneObject)
{
    if (auto areaLights = sceneObject->getAreaLights(); areaLights)
    {
        for (const auto& light : *areaLights)
            m_lights.push_back(&light);
    }
    m_sceneObject.emplace_back(std::move(sceneObject));
}

void Scene::addInfiniteLight(const std::shared_ptr<Light>& light)
{
    m_lights.push_back(light.get());
    m_infiniteLights.push_back(light.get());

    m_lightOwningPointers.push_back(light);
}

gsl::span<const std::unique_ptr<SceneObject>> Scene::getSceneObjects() const
{
    return m_sceneObject;
}

gsl::span<const Light* const> Scene::getLights() const
{
    return m_lights;
}

gsl::span<const Light* const> Scene::getInfiniteLights() const
{
    return m_infiniteLights;
}

}
