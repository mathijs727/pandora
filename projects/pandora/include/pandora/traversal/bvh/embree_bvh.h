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
    EmbreeBVH();
    EmbreeBVH(EmbreeBVH&&);
    ~EmbreeBVH();

    size_t size() const override final;

    void build(gsl::span<const LeafObj*> objects) override final;

    bool intersect(Ray& ray, RayHit& hitInfo) const override final;
    bool intersectAny(Ray& ray) const override final;

private:
    static void geometryBoundsFunc(const RTCBoundsFunctionArguments* args);
    static void geometryIntersectFunc(const RTCIntersectFunctionNArguments* args);
    static void geometryOccludedFunc(const RTCOccludedFunctionNArguments* args);

    static bool deviceMemoryMonitorFunction(void* userPtr, int64_t bytes, bool post);

private:
    RTCDevice m_device;
    RTCScene m_scene;
    std::atomic_size_t m_memoryUsed;

    static tbb::enumerable_thread_specific<gsl::span<RayHit>> s_intersectionDataRayHit;
};

}

#include "embree_bvh_impl.h"
