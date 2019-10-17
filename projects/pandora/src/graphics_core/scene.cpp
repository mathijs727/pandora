#include "pandora/graphics_core/scene.h"

namespace pandora {

Scene::Scene(std::shared_ptr<SceneNode>&& root, std::vector<std::unique_ptr<Light>>&& lights, std::vector<InfiniteLight*>&& infiniteLights)
    : root(root)
    , lights(std::move(lights))
    , infiniteLights(std::move(infiniteLights))
{
}

SceneBuilder::SceneBuilder()
    : m_pRoot(std::make_shared<SceneNode>())
{
}

std::shared_ptr<SceneObject> pandora::SceneBuilder::addSceneObject(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial)
{
    return std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, nullptr });
}

std::shared_ptr<SceneObject> pandora::SceneBuilder::addSceneObject(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial,
    std::unique_ptr<AreaLight>&& pAreaLight)
{
    return std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, pAreaLight.get() });
}

std::shared_ptr<SceneObject> pandora::SceneBuilder::addSceneObjectToRoot(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial)
{
    auto pSceneObject = addSceneObject(pShape, pMaterial);
    m_pRoot->objects.push_back(pSceneObject);
    return pSceneObject;
}

std::shared_ptr<SceneObject> pandora::SceneBuilder::addSceneObjectToRoot(
    std::shared_ptr<Shape> pShape,
    std::shared_ptr<Material> pMaterial,
    std::unique_ptr<AreaLight>&& pAreaLight)
{
    auto pSceneObject = addSceneObject(pShape, pMaterial, std::move(pAreaLight));
    m_pRoot->objects.push_back(pSceneObject);
    return pSceneObject;
}

void SceneBuilder::attachObject(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneObject> pSceneObject)
{
    pParent->objects.push_back(pSceneObject);
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNode()
{
    return std::make_shared<SceneNode>();
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNode(const glm::mat4& transform)
{
    auto pSceneNode = std::make_shared<SceneNode>();
    pSceneNode->transform = transform;
    return pSceneNode;
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNodeToRoot()
{
    auto pSceneNode = addSceneNode();
    attachNode(m_pRoot, pSceneNode);
    return pSceneNode;
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNodeToRoot(const glm::mat4& transform)
{
    auto pSceneNode = addSceneNode(transform);
    attachNode(m_pRoot, pSceneNode);
    return pSceneNode;
}

void SceneBuilder::attachNode(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneNode> pChild)
{
    pParent->children.push_back(pChild);
    pChild->pParent = pParent.get();
}

void SceneBuilder::makeRootNode(std::shared_ptr<SceneNode> pNewRoot)
{
    m_pRoot = pNewRoot;
}

void SceneBuilder::addInfiniteLight(std::unique_ptr<InfiniteLight>&& pInfiniteLight)
{
    m_infiniteLights.push_back(pInfiniteLight.get());
    m_lights.push_back(std::move(pInfiniteLight));
}

Scene SceneBuilder::build()
{
    attachLightRecurse(m_pRoot.get(), {});

    return Scene(std::move(m_pRoot), std::move(m_lights), std::move(m_infiniteLights));
}

void SceneBuilder::attachLightRecurse(SceneNode* pNode, std::optional<glm::mat4> transform)
{
    std::optional<glm::mat4> finalTransform = transform;
    if (pNode->transform) {
        if (finalTransform)
            finalTransform = (*pNode->transform) * (*finalTransform);
        else
            finalTransform = pNode->transform;
    }

    std::optional<glm::mat4> invFinalTransform;
    if (finalTransform)
        invFinalTransform = glm::inverse(*finalTransform);

    for (auto pChild : pNode->children)
        attachLightRecurse(pChild.get(), invFinalTransform);

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