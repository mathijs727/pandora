#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include <embree3/rtcore.h>
#include <gsl/gsl>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>
#include <variant>

namespace pandora {

template <typename LeafObj>
class EmbreeBVH : public BVH<LeafObj> {
public:
    EmbreeBVH();
    EmbreeBVH(EmbreeBVH&&);
    ~EmbreeBVH();

    void build(gsl::span<const LeafObj*> objects) override final;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
    bool intersect(Ray& ray, RayHit& hitInfo) const override final;

private:
    static void geometryBoundsFunc(const RTCBoundsFunctionArguments* args);
    static void geometryIntersectFunc(const RTCIntersectFunctionNArguments* args);

private:
    RTCDevice m_device;
    RTCScene m_scene;

    static tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<SurfaceInteraction>>> s_intersectionDataSI;
    static tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<RayHit>>> s_intersectionDataRayHit;
    static tbb::enumerable_thread_specific<bool> s_computeSurfaceInteraction;
};

}

#include "embree_bvh_impl.h"
