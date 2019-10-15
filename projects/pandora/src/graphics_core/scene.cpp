#include "pandora/graphics_core/scene.h"

namespace pandora {

Scene::Scene(SceneNode&& root, std::vector<std::unique_ptr<Light>>&& lights, std::vector<InfiniteLight*>&& infiniteLights)
    : root(root)
    , lights(std::move(lights))
    , infiniteLights(std::move(infiniteLights))
{
}

void pandora::SceneBuilder::addSceneObject(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial,
    SceneNode* pSceneNode)
{
    auto pSceneObject = std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, nullptr, pSceneNode });
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
    auto pSceneObject = std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, pAreaLight.get(), pSceneNode });
    if (pSceneNode)
        pSceneNode->objects.push_back(pSceneObject);
    else
        m_root.objects.push_back(pSceneObject);

    m_areaLights.push_back(pAreaLight.get());
    m_lights.push_back(std::move(pAreaLight));
}

SceneNode* SceneBuilder::addSceneNode(SceneNode* pParent)
{
    auto pSceneNode = std::make_shared<SceneNode>();
    pSceneNode->pParent = pParent;
    if (pParent)
        pParent->children.push_back(pSceneNode);
    else
        m_root.children.push_back(pSceneNode);

    return pSceneNode.get();
}

SceneNode* SceneBuilder::addSceneNode(const glm::mat4& transform, SceneNode* pParent)
{
    auto pSceneNode = std::make_shared<SceneNode>();
    pSceneNode->transform = transform;
    pSceneNode->pParent = pParent;
    if (pParent)
        pParent->children.push_back(pSceneNode);
    else
        m_root.children.push_back(pSceneNode);

    return pSceneNode.get();
}

Scene SceneBuilder::build()
{
    attachLightRecurse(&m_root, {});

    return Scene(std::move(m_root), std::move(m_lights), std::move(m_infiniteLights));
}

void SceneBuilder::attachLightRecurse(SceneNode* pNode, std::optional<glm::mat4> transform)
{
    std::optional<glm::mat4> finalTransform = transform;
    if (pNode->transform) {
        if (finalTransform)
            finalTransform = (*finalTransform) * (*pNode->transform);
        else
            finalTransform = pNode->transform;
    }

    for (auto pChild : pNode->children)
        attachLightRecurse(pChild.get(), finalTransform);

    for (auto pObject : pNode->objects) {
        if (pObject->pAreaLight) {
            if (finalTransform)
                pObject->pAreaLight->attachToShape(pObject->pShape.get(), *finalTransform);
            else
                pObject->pAreaLight->attachToShape(pObject->pShape.get());
        }
    }
}

}