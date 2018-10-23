#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include <embree3/rtcore.h>
#include <gsl/gsl>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>
#include <variant>
#include <atomic>

namespace pandora {

template <typename LeafObj>
class EmbreeBVH : public BVH<LeafObj> {
public:
    EmbreeBVH(gsl::span<LeafObj> objects);
    EmbreeBVH(EmbreeBVH&&);
    ~EmbreeBVH();

    size_t sizeBytes() const override final;

    void intersect(gsl::span<Ray> rays, gsl::span<RayHit> hitInfos) const override final;
    void intersectAny(gsl::span<Ray> rays) const override final;

private:
    static void geometryBoundsFunc(const RTCBoundsFunctionArguments* args);
    static void geometryIntersectFunc(const RTCIntersectFunctionNArguments* args);
    static void geometryOccludedFunc(const RTCOccludedFunctionNArguments* args);

    static bool deviceMemoryMonitorFunction(void* userPtr, int64_t bytes, bool post);

private:
    RTCDevice m_device;
    RTCScene m_scene;
    std::atomic_size_t m_memoryUsed;

    std::vector<LeafObj> m_leafs;
    static tbb::enumerable_thread_specific<gsl::span<RayHit>> s_intersectionDataRayHit;
};

}

#include "embree_bvh_impl.h"
