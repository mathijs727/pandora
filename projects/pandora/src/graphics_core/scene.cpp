#include "pandora/graphics_core/scene.h"

namespace pandora {

Scene::Scene(SceneNode&& root, std::vector<AreaLightInstance>&& areaLightInstances)
    : root(root)
    , areaLightInstances(areaLightInstances)
{
}

void pandora::SceneBuilder::addSceneObject(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial,
    SceneNode* pSceneNode)
{
    auto pSceneObject = std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, nullptr });
    if (pSceneNode)
        pSceneNode->objects.push_back(pSceneObject);
    else
        m_root.objects.push_back(pSceneObject);
}

void pandora::SceneBuilder::addSceneObject(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial,
    std::unique_ptr<AreaLight>&& pAreaLight,
    SceneNode* pSceneNode)
{
    auto pSceneObject = std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, std::move(pAreaLight) });
    if (pSceneNode)
        pSceneNode->objects.push_back(pSceneObject);
    else
        m_root.objects.push_back(pSceneObject);

    Scene::AreaLightInstance areaLightInstance { {}, pSceneObject->pAreaLight.get() };
    m_areaLightInstances.push_back(areaLightInstance);
}

SceneNode* SceneBuilder::addSceneNode(SceneNode* pParent)
{
    auto pSceneNode = std::make_shared<SceneNode>();
    if (pParent)
        pParent->children.push_back(pSceneNode);
    else
        m_root.children.push_back(pSceneNode);

    return pSceneNode.get();
}

SceneNode* SceneBuilder::addSceneNode(const pandora::Transform& transform, SceneNode* pParent)
{
    auto pSceneNode = std::make_shared<SceneNode>(SceneNode { transform, {}, {} });
    if (pParent)
        pParent->children.push_back(pSceneNode);
    else
        m_root.children.push_back(pSceneNode);

    return pSceneNode.get();
}

Scene SceneBuilder::build()
{
    return Scene(std::move(m_root), std::move(m_areaLightInstances));
}

}