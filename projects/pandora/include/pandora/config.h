#pragma once
#include "pandora/core/pandora.h"

namespace pandora
{

constexpr bool OUT_OF_CORE_ACCELERATION_STRUCTURE = true;
template <typename T>
using AccelerationStructure = OOCBatchingAccelerationStructure<T, 8>;
//using AccelerationStructure = InCoreAccelerationStructure<T>;
//using AccelerationStructure = InCoreBatchingAccelerationStructure<T, 400>;

constexpr int PARALLEL_PATHS = 16 * 1000 * 1000;
constexpr unsigned OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF = 35000;
constexpr bool OUT_OF_CORE_OCCLUSION_CULLING = false;
constexpr bool OUT_OF_CORE_DISABLE_FILE_CACHING = true;
constexpr size_t OUT_OF_CORE_MEMORY_LIMIT = 1024llu * 1024llu * 350llu;
constexpr size_t OUT_OF_CORE_SVDAG_RESOLUTION = 256;

// Use random seeds or deterministic seeds (so images can be reproduced)
constexpr bool USE_RANDOM_SEEDS = false;

// Whether to enable statistics that are relatively expensive to measure
constexpr bool ENABLE_ADDITIONAL_STATISTICS = false;

template<typename... T>
using CacheT = LRUCache<T...>;

}