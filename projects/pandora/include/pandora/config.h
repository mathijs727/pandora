#pragma once
#include "pandora/core/pandora.h"

namespace pandora
{

constexpr bool OUT_OF_CORE_ACCELERATION_STRUCTURE = false;
template <typename T>
using AccelerationStructure = InCoreAccelerationStructure<T>;
//using AccelerationStructure = OOCBatchingAccelerationStructure<T, LRUCache, 400>;
//using AccelerationStructure = InCoreBatchingAccelerationStructure<T, 400>;

constexpr int PARALLEL_SAMPLES = 2;
constexpr unsigned OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF = 100000;
constexpr bool OUT_OF_CORE_OCCLUSION_CULLING = false;
constexpr bool OUT_OF_CORE_DISABLE_FILE_CACHING = false;
constexpr size_t OUT_OF_CORE_MEMORY_LIMIT = 1024llu * 1024llu * 1024llu * 128llu;
constexpr size_t OUT_OF_CORE_SVDAG_RESOLUTION = 64;

constexpr bool ISLAND_WATER_MATERIAL = true;// Give the water in the Island scene a translucent material

// Use random seeds or deterministic seeds (so images can be reproduced)
constexpr bool USE_RANDOM_SEEDS = true;


}