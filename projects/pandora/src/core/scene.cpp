#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"

namespace pandora {

void Scene::addSceneObject(SceneObject&& sceneNode)
{
    m_sceneObject.emplace_back(std::move(sceneNode));
}

void Scene::addInfiniteLight(const std::shared_ptr<Light>& light)
{
    m_lights.push_back(light);
    m_infiniteLights.push_back(light);
}

gsl::span<const SceneObject> Scene::getSceneObjects() const
{
    return m_sceneObject;
}

gsl::span<const std::shared_ptr<Light>> Scene::getLights() const
{
    return m_lights;
}

gsl::span<const std::shared_ptr<Light>> Scene::getInfiniteLIghts() const
{
    return m_infiniteLights;
}

}
