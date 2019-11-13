#pragma once
#include "pandora/graphics_core/render_config.h"
#include <filesystem>
#include <glm/glm.hpp>
#include <string_view>

namespace pandora {

RenderConfig loadFromFile(std::filesystem::path filePath, bool loadMaterials = true);
//RenderConfig loadFromFileOOC(std::filesystem::path filePath, bool loadMaterials = true);

}
