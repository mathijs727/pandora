#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"

namespace pandora {

void Scene::addSceneObject(SceneObject&& sceneNode)
{
    m_sceneObject.emplace_back(std::move(sceneNode));
}

gsl::span<const SceneObject> Scene::getSceneObjects() const
{
    return m_sceneObject;
}

}
