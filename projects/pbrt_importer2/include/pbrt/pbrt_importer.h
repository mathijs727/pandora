#pragma once
#include <filesystem>
#include <pandora/graphics_core/load_from_file.h>
#include <stream/cache/cache.h>

namespace pbrt {

pandora::RenderConfig loadFromPBRTFile(std::filesystem::path filePath, stream::CacheBuilder* pCacheBuilder = nullptr, bool loadTextures = false);

}