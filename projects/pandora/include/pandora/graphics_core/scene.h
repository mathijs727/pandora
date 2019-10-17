#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/lights/area_light.h"
#include "pandora/shapes/forward_declares.h"
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace pandora {

struct SceneObject {
    std::shared_ptr<Shape> pShape;
    std::shared_ptr<Material> pMaterial;
    AreaLight* pAreaLight;

    SceneNode* pParent;
};

struct SceneNode {
    std::optional<glm::mat4> transform;
    std::vector<std::shared_ptr<SceneNode>> children;
    std::vector<std::shared_ptr<SceneObject>> objects;

    SceneNode* pParent;
};

struct Scene {
    std::shared_ptr<SceneNode> root;
    std::vector<std::unique_ptr<Light>> lights;
    std::vector<InfiniteLight*> infiniteLights;

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

    std::shared_ptr<SceneNode> addSceneNode();
    std::shared_ptr<SceneNode> addSceneNode(const glm::mat4& transform);
    std::shared_ptr<SceneNode> addSceneNodeToRoot();
    std::shared_ptr<SceneNode> addSceneNodeToRoot(const glm::mat4& transform);
    void attachNode(std::shared_ptr<SceneNode> pParent, std::shared_ptr<SceneNode> pChild);

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