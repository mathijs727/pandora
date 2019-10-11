#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/transform.h"
#include "pandora/lights/area_light.h"
#include "pandora/scene/scene_object_ref.h"
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
    SceneNode m_root;
};

}