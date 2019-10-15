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
    SceneNode root;
    std::vector<std::unique_ptr<Light>> lights;
    std::vector<InfiniteLight*> infiniteLights;

private:
    friend class SceneBuilder;
    Scene(SceneNode&& root, std::vector<std::unique_ptr<Light>>&& lights, std::vector<InfiniteLight*>&& infiniteLights);
};

class SceneBuilder {
public:
    void addSceneObject(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial, SceneNode* pSceneNode = nullptr);
    void addSceneObject(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial, std::unique_ptr<AreaLight>&& pAreaLight, SceneNode* pSceneNode = nullptr);

    SceneNode* addSceneNode(SceneNode* pParent = nullptr);
    SceneNode* addSceneNode(const glm::mat4& transform, SceneNode* pParent = nullptr);

    void addInfiniteLight(std::unique_ptr<InfiniteLight>&& pInfiniteLight);

    Scene build();

private:
    void attachLightRecurse(SceneNode* pNode, std::optional<glm::mat4> transform);

private:
    SceneNode m_root;
    std::vector<std::unique_ptr<Light>> m_lights;
    std::vector<InfiniteLight*> m_infiniteLights;

    std::vector<AreaLight*> m_areaLights;
};

}