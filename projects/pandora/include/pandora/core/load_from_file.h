#pragma once
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include <glm/glm.hpp>
#include <string_view>
#include <filesystem>

namespace pandora {

struct RenderConfig {
    RenderConfig() = default;
    RenderConfig(size_t goemetryCacheSize)
        : scene(goemetryCacheSize)
    {
    }
    std::unique_ptr<PerspectiveCamera> camera;
    glm::ivec2 resolution;
    Scene scene;
};

RenderConfig loadFromFile(std::filesystem::path filePath , bool loadMaterials = true);
RenderConfig loadFromFileOOC(std::filesystem::path filePath, bool loadMaterials = true);

}
