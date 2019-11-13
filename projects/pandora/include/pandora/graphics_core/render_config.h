#pragma once
#include "pandora/graphics_core/pandora.h"
#include <glm/vec2.hpp>
#include <memory>

namespace pandora {
struct RenderConfig {
    std::unique_ptr<Scene> pScene;
    std::unique_ptr<PerspectiveCamera> camera;
    glm::ivec2 resolution;
};
}