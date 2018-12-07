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
constexpr int CUTOFF_RAY_COUNT = 0;// Stop processing when the ray count drops below this number
constexpr unsigned OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF = 10000;
constexpr bool OUT_OF_CORE_OCCLUSION_CULLING = true;
constexpr bool OUT_OF_CORE_DISABLE_FILE_CACHING = false;
constexpr size_t OUT_OF_CORE_MEMORY_LIMIT = 1024llu * 1024llu * 1000llu;
constexpr size_t OUT_OF_CORE_SVDAG_RESOLUTION = 64;

// Subdivide the scene to make it artificially more complex
constexpr int SUBDIVIDE_LEVEL = 1;

// Use random seeds or deterministic seeds (so images can be reproduced)
constexpr bool USE_RANDOM_SEEDS = false;

// Whether to enable statistics that are relatively expensive to measure
constexpr bool ENABLE_ADDITIONAL_STATISTICS = true;

template<typename... T>
using CacheT = LRUCache<T...>;

}