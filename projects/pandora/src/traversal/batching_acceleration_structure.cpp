#include "pandora/traversal/batching_acceleration_structure.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/enumerate.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <deque>
#include <embree3/rtcore.h>
#include <memory>
#include <optick.h>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_vector.h>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace pandora {

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);

BatchingAccelerationStructureBuilder::BatchingAccelerationStructureBuilder(
    const Scene* pScene,
    tasking::LRUCacheTS* pCache,
    tasking::TaskGraph* pTaskGraph,
    unsigned primitivesPerBatchingPoint,
    size_t botLevelBVHCacheSize,
    unsigned svdagRes)
    : m_pScene(pScene)
    , m_pGeometryCache(pCache)
    , m_pTaskGraph(pTaskGraph)
    , m_primitivesPerBatchingPoint(primitivesPerBatchingPoint)
    , m_botLevelBVHCacheSize(botLevelBVHCacheSize)
    , m_svdagRes(svdagRes)
{
}

void BatchingAccelerationStructureBuilder::preprocessScene(Scene& scene, tasking::LRUCacheTS& oldCache, tasking::CacheBuilder& newCacheBuilder, unsigned primitivesPerBatchingPoint)
{
    OPTICK_EVENT();

    // NOTE: modifies pScene in place
    //
    // Split large shapes into smaller sub shapes so we can guarantee that the batching poinst never exceed the given size.
    // This should also help with reducing the spatial extent of the batching points by (hopefully) splitting spatially large shapes.
    spdlog::info("Splitting large scene objects");
    RTCDevice embreeDevice = rtcNewDevice(nullptr);
    detail::splitLargeSceneObjects(scene.pRoot.get(), oldCache, newCacheBuilder, embreeDevice, primitivesPerBatchingPoint / 8);
    rtcReleaseDevice(embreeDevice);
}

void BatchingAccelerationStructureBuilder::setEmbreeErrorFunc(RTCDevice embreeDevice)
{
    rtcSetDeviceErrorFunction(embreeDevice, embreeErrorFunc, nullptr);
}

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
{
    switch (code) {
    case RTC_ERROR_NONE:
        spdlog::error("RTC_ERROR_NONE {}", str);
        break;
    case RTC_ERROR_UNKNOWN:
        spdlog::error("RTC_ERROR_UNKNOWN {}", str);
        break;
    case RTC_ERROR_INVALID_ARGUMENT:
        spdlog::error("RTC_ERROR_INVALID_ARGUMENT {}", str);
        break;
    case RTC_ERROR_INVALID_OPERATION:
        spdlog::error("RTC_ERROR_INVALID_OPERATION {}", str);
        break;
    case RTC_ERROR_OUT_OF_MEMORY:
        spdlog::error("RTC_ERROR_OUT_OF_MEMORY {}", str);
        break;
    case RTC_ERROR_UNSUPPORTED_CPU:
        spdlog::error("RTC_ERROR_UNSUPPORTED_CPU {}", str);
        break;
    case RTC_ERROR_CANCELLED:
        spdlog::error("RTC_ERROR_CANCELLED {}", str);
        break;
    }
}
}