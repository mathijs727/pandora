#include "pandora/graphics_core/scene.h"
#include "pandora/core/stats.h"
#include "pandora/utility/error_handling.h"
#include <unordered_set>

namespace pandora {

Bounds SceneNode::computeBounds() const
{
    Bounds bounds;
    for (const auto& pSceneObject : objects) {
        bounds.extend(pSceneObject->pShape->getBounds());
    }

    for (const auto& childAndTransform : children) {
        const auto& [pChild, transformOpt] = childAndTransform;
        Bounds childBounds = pChild->computeBounds();
        if (transformOpt)
            childBounds *= transformOpt.value();
        bounds.extend(childBounds);
    }
    return bounds;
}

Scene::Scene(std::shared_ptr<SceneNode>&& root, std::vector<std::unique_ptr<Light>>&& lights, std::vector<InfiniteLight*>&& infiniteLights)
    : pRoot(root)
    , lights(std::move(lights))
    , infiniteLights(std::move(infiniteLights))
{
    //ALWAYS_ASSERT(g_stats.scene.uniquePrimitives == 0);
    //ALWAYS_ASSERT(g_stats.scene.totalPrimitives == 0);
    g_stats.scene.uniquePrimitives = countUniquePrimitives();
    g_stats.scene.totalPrimitives = countInstancedPrimitives();
}

size_t Scene::countUniquePrimitives() const
{
    std::unordered_set<const Shape*> visitedShapes;

    std::function<size_t(const SceneNode*)> countRecurse = [&](const SceneNode* pSceneNode) {
        size_t count = 0;
        for (const auto& pSceneObject : pSceneNode->objects) {
            const auto* pShape = pSceneObject->pShape.get();
            if (visitedShapes.find(pShape) == std::end(visitedShapes)) {
                visitedShapes.insert(pShape);
                count += pShape->numPrimitives();
            }
        }
        for (const auto& [pChild, _] : pSceneNode->children)
            count += countRecurse(pChild.get());
        return count;
    };
    return countRecurse(pRoot.get());
}

size_t Scene::countInstancedPrimitives() const
{
    std::function<size_t(const SceneNode*)> countRecurse = [&](const SceneNode* pSceneNode) {
        size_t count = 0;
        for (const auto& pSceneObject : pSceneNode->objects)
            count += pSceneObject->pShape->numPrimitives();
        for (const auto& [pChild, _] : pSceneNode->children)
            count += countRecurse(pChild.get());
        return count;
    };
    return countRecurse(pRoot.get());
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
    auto* pAreaLightNonOwning = pAreaLight.get();
    m_lights.push_back(std::move(pAreaLight));
    return std::make_shared<SceneObject>(SceneObject { pShape, pMaterial, pAreaLightNonOwning });
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

void SceneBuilder::attachObjectToRoot(std::shared_ptr<SceneObject> pSceneObject)
{
    m_pRoot->objects.push_back(pSceneObject);
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNode()
{
    return std::make_shared<SceneNode>();
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNodeToRoot()
{
    auto pSceneNode = addSceneNode();
    attachNode(m_pRoot, pSceneNode);
    return pSceneNode;
}

std::shared_ptr<SceneNode> SceneBuilder::addSceneNodeToRoot(const glm::mat4& transform)
{
    auto pSceneNode = addSceneNode();
    attachNode(m_pRoot, pSceneNode, transform);
    return pSceneNode;
}

void SceneBuilder::attachNode(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneNode> pChild)
{
    pParent->children.push_back({ pChild, {} });
    pChild->pParent = pParent.get();
}

void SceneBuilder::attachNode(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneNode> pChild, const glm::mat4& transform)
{
    pParent->children.push_back({ pChild, transform });
    pChild->pParent = pParent.get();
}

void SceneBuilder::attachNodeToRoot(std::shared_ptr<SceneNode> pChild)
{
    attachNode(m_pRoot, pChild);
}

void SceneBuilder::attachNodeToRoot(std::shared_ptr<SceneNode> pChild, const glm::mat4& transform)
{
    attachNode(m_pRoot, pChild, transform);
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
    // Attach area lights at this node
    for (auto pObject : pNode->objects) {
        if (pObject->pAreaLight) {
            if (transform)
                pObject->pAreaLight->attachToShape(pObject->pShape.get(), *transform);
            else
                pObject->pAreaLight->attachToShape(pObject->pShape.get());
        }
    }

    // Recurse to children
    for (auto [pChild, childTransform] : pNode->children) {
        std::optional<glm::mat4> totalTransform = transform;
        if (childTransform) {
            if (totalTransform)
                totalTransform = (*childTransform) * (*totalTransform);
            else
                totalTransform = childTransform;
        }
        attachLightRecurse(pChild.get(), totalTransform);
    }
}

}