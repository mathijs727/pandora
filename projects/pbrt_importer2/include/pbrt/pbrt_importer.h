#pragma once
#include <filesystem>
#include <pandora/graphics_core/load_from_file.h>

namespace pbrt {

pandora::RenderConfig loadFromPBRTFile(std::filesystem::path filePath);

}