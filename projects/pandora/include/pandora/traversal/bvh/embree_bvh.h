#include "pandora/traversal/bvh.h"
#include <embree3/rtcore.h>
#include <gsl/gsl>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>

namespace pandora
{

template <typename LeafObj>
class EmbreeBVH : public BVH<LeafObj>
{
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
    static bool m_computeSurfaceInteractions;
    static tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<SurfaceInteraction>>> m_intersectRayData;
    static tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<RayHit>>> m_intersectRayData2;

	RTCDevice m_device;
	RTCScene m_scene;
};

}

#include "embree_bvh_impl.h"

