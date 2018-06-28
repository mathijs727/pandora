#include "pandora/traversal/bvh.h"
#include <embree3/rtcore.h>
#include <gsl/gsl>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>

namespace pandora
{

template <typename LeafNode>
class EmbreeBVH : public BVH<LeafNode>
{
public:
	EmbreeBVH();
	~EmbreeBVH();

	void addPrimitive(const LeafNode& leaf) override final;
	void commit() override final;

	bool intersect(Ray& ray, SurfaceInteraction& si) const override final;

private:
	static void geometryBoundsFunc(const RTCBoundsFunctionArguments* args);
	static void geometryIntersectFunc(const RTCIntersectFunctionNArguments* args);
private:
	static tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<SurfaceInteraction>>> m_intersectRayData;

	RTCDevice m_device;
	RTCScene m_scene;
};

}

#include "embree_bvh_impl.h"

