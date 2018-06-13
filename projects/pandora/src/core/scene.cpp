#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"

namespace pandora {

void Scene::addSceneObject(const SceneObject& sceneNode)
{
    m_sceneObject.push_back(sceneNode);
}

gsl::span<const SceneObject> Scene::getSceneObjects() const
{
    return m_sceneObject;
}

}
