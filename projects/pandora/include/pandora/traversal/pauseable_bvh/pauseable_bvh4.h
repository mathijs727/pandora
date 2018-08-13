#pragma once
#include "pandora/traversal/pauseable_bvh.h"
#include "pandora/utility/simd/simd4.h"
#include <embree3/rtcore.h>

namespace pandora {

template <typename LeafObj>
class PauseableBVH4 : PauseableBVH<LeafObj> {
public:
	PauseableBVH4(gsl::span<const LeafObj> object);
	~PauseableBVH4();

	bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
	bool intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle handle) const override final;
private:
	static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
	static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
	static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);

	static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);
private:
	struct alignas(16) BVHNode {
		simd::vec4_f32 boundsMinX;
		simd::vec4_f32 boundsMinY;
		simd::vec4_f32 boundsMinZ;
		simd::vec4_f32 boundsMaxX;
		simd::vec4_f32 boundsMaxY;
		simd::vec4_f32 boundsMaxZ;
		uint32_t firstChildOffset;
		uint32_t firstLeafOffset;
		struct {
			uint32_t validMask : 4;
			uint32_t leafMask : 4;
			uint32_t depth : 8;
		};
		uint32_t parentOffset;
	};

	static_assert(sizeof(BVHNode) < 128);
};

}
