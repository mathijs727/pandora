#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/lights/area_light.h"
#include "pandora/shapes/forward_declares.h"
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace pandora {

struct SceneObject {
    std::shared_ptr<Shape> pShape;
    std::shared_ptr<Material> pMaterial;
    AreaLight* pAreaLight;

    SceneNode* pParent;
};

struct SceneNode {
    std::vector<std::pair<std::shared_ptr<SceneNode>, std::optional<glm::mat4>>> children;
    std::vector<std::shared_ptr<SceneObject>> objects;

    SceneNode* pParent;

public:
    Bounds computeBounds() const;
};

struct Scene {
    std::shared_ptr<SceneNode> pRoot;
    std::vector<std::unique_ptr<Light>> lights;
    std::vector<InfiniteLight*> infiniteLights;

public:
    size_t countUniquePrimitives() const;
    size_t countInstancedPrimitives() const;

private:
    friend class SceneBuilder;
    Scene(std::shared_ptr<SceneNode>&& root, std::vector<std::unique_ptr<Light>>&& lights, std::vector<InfiniteLight*>&& infiniteLights);
};

class SceneBuilder {
public:
    SceneBuilder();

    std::shared_ptr<SceneObject> addSceneObject(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial);
    std::shared_ptr<SceneObject> addSceneObject(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial, std::unique_ptr<AreaLight>&& pAreaLight);
    std::shared_ptr<SceneObject> addSceneObjectToRoot(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial);
    std::shared_ptr<SceneObject> addSceneObjectToRoot(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial, std::unique_ptr<AreaLight>&& pAreaLight);
    void attachObject(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneObject> pSceneObject);
    void attachObjectToRoot(std::shared_ptr<SceneObject> pSceneObject);

    std::shared_ptr<SceneNode> addSceneNode();
    std::shared_ptr<SceneNode> addSceneNodeToRoot();
    std::shared_ptr<SceneNode> addSceneNodeToRoot(const glm::mat4& transform);
    void attachNode(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneNode> pChild);
    void attachNode(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneNode> pChild, const glm::mat4& transform);
    void attachNodeToRoot(std::shared_ptr<SceneNode> pChild);
    void attachNodeToRoot(std::shared_ptr<SceneNode> pChild, const glm::mat4& transform);

    void makeRootNode(std::shared_ptr<SceneNode> pNewRoot);

    void addInfiniteLight(std::unique_ptr<InfiniteLight>&& pInfiniteLight);

    Scene build();

private:
    void attachLightRecurse(SceneNode* pNode, std::optional<glm::mat4> transform);

private:
    std::shared_ptr<SceneNode> m_pRoot;
    std::vector<std::unique_ptr<Light>> m_lights;
    std::vector<InfiniteLight*> m_infiniteLights;

    std::vector<AreaLight*> m_areaLights;
};

}