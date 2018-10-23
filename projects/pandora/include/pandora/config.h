#pragma once
#include "pandora/core/pandora.h"

namespace pandora
{

constexpr bool OUT_OF_CORE_ACCELERATION_STRUCTURE = true;
template <typename T>
using AccelerationStructure = OOCBatchingAccelerationStructure<T, LRUCache, 400>;
//using AccelerationStructure = InCoreBatchingAccelerationStructure<T, 400>;
//using AccelerationStructure = InCoreAccelerationStructure<T>;

static constexpr unsigned OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF = 100000;
static constexpr bool OUT_OF_CORE_OCCLUSION_CULLING = false;
static constexpr bool OUT_OF_CORE_DISABLE_FILE_CACHING = false;
static constexpr size_t OUT_OF_CORE_MEMORY_LIMIT = 1024llu * 1024llu * 1024llu * 128llu;
static constexpr size_t OUT_OF_CORE_SVDAG_RESOLUTION = 64;

}