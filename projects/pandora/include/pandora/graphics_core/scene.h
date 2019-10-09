#pragma once
#include "pandora/geometry/forward_declares.h"
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/transform.h"
#include "pandora/scene/scene_object_ref.h"
#include <memory>
#include <optional>
#include <vector>

namespace pandora {

struct SceneObject {
    std::shared_ptr<TriangleMesh> pGeometry;
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