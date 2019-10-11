#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/transform.h"
#include "pandora/lights/area_light.h"
#include "pandora/shapes/forward_declares.h"
#include <memory>
#include <optional>
#include <vector>

namespace pandora {

struct SceneObject {
    std::shared_ptr<Shape> pShape;
    std::shared_ptr<Material> pMaterial;
    std::unique_ptr<AreaLight> pAreaLight;
};

struct SceneNode {
    std::optional<pandora::Transform> transform;
    std::vector<std::shared_ptr<SceneNode>> children;
    std::vector<std::shared_ptr<SceneObject>> objects;
};

struct Scene {
    struct AreaLightInstance {
        std::optional<glm::mat4> transform;
        AreaLight* pAreaLight;
    };

    SceneNode root;
    std::vector<Scene::AreaLightInstance> areaLightInstances;

private:
    friend class SceneBuilder;
    Scene(SceneNode&& root, std::vector<AreaLightInstance>&& areaLightInstances);
};

class SceneBuilder {
public:
    void addSceneObject(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial, SceneNode* pSceneNode = nullptr);
    void addSceneObject(std::shared_ptr<Shape> pShape, std::shared_ptr<Material> pMaterial, std::unique_ptr<AreaLight>&& pAreaLight, SceneNode* pSceneNode = nullptr);

    SceneNode* addSceneNode(SceneNode* pParent = nullptr);
    SceneNode* addSceneNode(const pandora::Transform& transform, SceneNode* pParent = nullptr);

    Scene build();

private:
    SceneNode m_root;
    std::vector<Scene::AreaLightInstance> m_areaLightInstances;
};

}