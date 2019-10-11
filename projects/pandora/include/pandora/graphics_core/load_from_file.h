#pragma once
#include "pandora/graphics_core/perspective_camera.h"
#include "pandora/graphics_core/scene.h"
#include <glm/glm.hpp>
#include <string_view>
#include <filesystem>

namespace pandora {

struct RenderConfig {
    std::unique_ptr<PerspectiveCamera> camera;
    glm::ivec2 resolution;
    std::unique_ptr<Scene> pScene;
};

RenderConfig loadFromFile(std::filesystem::path filePath , bool loadMaterials = true);
RenderConfig loadFromFileOOC(std::filesystem::path filePath, bool loadMaterials = true);

}
