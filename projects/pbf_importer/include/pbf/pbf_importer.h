#pragma once
#include <filesystem>
#include <pandora/graphics_core/load_from_file.h>
#include <stream/cache/cache.h>

namespace pbf {

pandora::RenderConfig loadFromPBFFile(std::filesystem::path filePath, tasking::CacheBuilder* pCacheBuilder = nullptr, bool loadTextures = false);

}