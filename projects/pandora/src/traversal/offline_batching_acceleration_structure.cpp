#include "pandora/traversal/offline_batching_acceleration_structure.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/traversal/batching.h"
#include "pandora/utility/enumerate.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <deque>
#include <embree3/rtcore.h>
#include <memory>
#include <optick.h>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_vector.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pandora {

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);

OfflineBatchingAccelerationStructureBuilder::OfflineBatchingAccelerationStructureBuilder(
    const Scene* pScene,
    tasking::LRUCacheTS* pCache,
    tasking::TaskGraph* pTaskGraph,
    unsigned primitivesPerBatchingPoint,
    size_t botLevelBVHCacheSize,
    unsigned svdagRes)
    : m_botLevelBVHCacheSize(botLevelBVHCacheSize)
    , m_svdagRes(svdagRes)
    , m_pGeometryCache(pCache)
    , m_pTaskGraph(pTaskGraph)
{
    spdlog::info("Splitting scene into sub scenes");
    RTCDevice embreeDevice = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(embreeDevice, embreeErrorFunc, nullptr);
    auto subScenes = detail::createSubScenes(*pScene, primitivesPerBatchingPoint, embreeDevice);
    rtcReleaseDevice(embreeDevice);

    m_subScenes.resize(subScenes.size());
    std::transform(std::begin(subScenes), std::end(subScenes), std::begin(m_subScenes),
        [](auto& subScene) { return std::make_unique<SubScene>(std::move(subScene)); });
}

void OfflineBatchingAccelerationStructureBuilder::preprocessScene(Scene& scene, tasking::LRUCacheTS& oldCache, tasking::CacheBuilder& newCacheBuilder, unsigned primitivesPerBatchingPoint)
{
    OPTICK_EVENT();

    // NOTE: modifies pScene in place
    //
    // Split large shapes into smaller sub shpaes so we can guarantee that the batching poinst never exceed the given size.
    // This should also help with reducing the spatial extent of the batching points by (hopefully) splitting spatially large shapes.
    spdlog::info("Splitting large scene objects");
    RTCDevice embreeDevice = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(embreeDevice, embreeErrorFunc, nullptr);
    detail::splitLargeSceneObjects(scene.pRoot.get(), oldCache, newCacheBuilder, embreeDevice, primitivesPerBatchingPoint / 8);
    rtcReleaseDevice(embreeDevice);
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