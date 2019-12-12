#pragma once
#include <vector>
#include <tuple>
#include <optional>
#include <glm/glm.hpp>
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"

namespace pandora {
struct SubScene {
    std::vector<std::pair<SceneNode*, std::optional<glm::mat4>>> sceneNodes;
    std::vector<SceneObject*> sceneObjects;

public:
    Bounds computeBounds() const;
};

}