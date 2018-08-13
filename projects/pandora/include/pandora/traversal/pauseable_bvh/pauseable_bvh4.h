#pragma once
#include "pandora/traversal/pauseable_bvh.h"
#include "pandora/utility/simd/simd4.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include <embree3/rtcore.h>
#include <tuple>

namespace pandora {

template <typename LeafObj>
class PauseableBVH4 : PauseableBVH<LeafObj> {
public:
	PauseableBVH4(gsl::span<const LeafObj> object, gsl::span<const Bounds> bounds);
	PauseableBVH4(PauseableBVH4&&) = default;
    ~PauseableBVH4() = default;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
    bool intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle handle) const override final;

private:
	static void* encodeBVHConstructionLeafHandle(uint32_t handle);
	static void* encodeBVHConstructionInnerNodeHandle(uint32_t handle);
	static std::tuple<bool, uint32_t> decodeBVHConstructionHandle(void* handle);

    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

private:
    struct alignas(16) BVHNode {
        simd::vec4_f32 minX;
        simd::vec4_f32 minY;
        simd::vec4_f32 minZ;
        simd::vec4_f32 maxX;
        simd::vec4_f32 maxY;
        simd::vec4_f32 maxZ;
        std::array<uint32_t, 4> childrenHandles;
        uint32_t parentHandle;
        /*struct {
            uint32_t validMask : 4;
            uint32_t leafMask : 4;
            uint32_t depth : 8;
        };*/
		uint8_t validMask;
		uint8_t leafMask;
		uint8_t depth;
    };

	ContiguousAllocatorTS<typename PauseableBVH4::BVHNode> m_innerNodeAllocator;
	ContiguousAllocatorTS<LeafObj> m_leafAllocator;
	gsl::span<const LeafObj> m_tmpConstructionLeafs;

    static_assert(sizeof(BVHNode) <= 128);
};

template <typename LeafObj>
inline PauseableBVH4<LeafObj>::PauseableBVH4(gsl::span<const LeafObj> objects, gsl::span<const Bounds> objectsBounds) :
	m_innerNodeAllocator(objects.size()),
	m_leafAllocator(objects.size())
{
	assert(objects.size() == bounds.size());

	// Copy leafs
	m_tmpConstructionLeafs = objects;

	// Create a representatin of the leafs that Embree will understand
	std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
	embreeBuildPrimitives.reserve(objects.size());
	for (unsigned i = 0; i < static_cast<unsigned>(objects.size()); i++) {
		auto bounds = objectsBounds[i];

		RTCBuildPrimitive primitive;
		primitive.lower_x = bounds.min.x;
		primitive.lower_y = bounds.min.y;
		primitive.lower_z = bounds.min.z;
		primitive.upper_x = bounds.max.x;
		primitive.upper_y = bounds.max.y;
		primitive.upper_z = bounds.max.z;
		primitive.geomID = 0;
		primitive.primID = i;
		embreeBuildPrimitives.push_back(primitive);
	}

	// Build the BVH using the Embree BVH builder API
	RTCDevice device = rtcNewDevice(nullptr);
	rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);
	RTCBVH bvh = rtcNewBVH(device);

	RTCBuildArguments arguments = rtcDefaultBuildArguments();
	arguments.byteSize = sizeof(arguments);
	arguments.buildFlags = RTC_BUILD_FLAG_NONE;
	arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
	arguments.maxBranchingFactor = 4;
	arguments.minLeafSize = 1;
	arguments.maxLeafSize = 1;
	arguments.bvh = bvh;
	arguments.primitives = embreeBuildPrimitives.data();
	arguments.primitiveCount = embreeBuildPrimitives.size();
	arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
	arguments.createNode = innerNodeCreate;
	arguments.setNodeChildren = innerNodeSetChildren;
	arguments.setNodeBounds = innerNodeSetBounds;
	arguments.createLeaf = leafCreate;
	arguments.userPtr = this;
	rtcBuildBVH(&arguments);

	// Releases Embree memory (including the temporary BVH)
	rtcReleaseBVH(bvh);
	rtcReleaseDevice(device);

	m_leafAllocator.compact();
	m_innerNodeAllocator.compact();
}

template <typename LeafObj>
inline bool PauseableBVH4<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    return false;
}

template <typename LeafObj>
inline bool PauseableBVH4<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle handle) const
{
	struct SIMDRay {
		simd::vec4_f32 originX;
		simd::vec4_f32 originY;
		simd::vec4_f32 originZ;

		simd::vec4_f32 invDirectionX;
		simd::vec4_f32 invDirectionY;
		simd::vec4_f32 invDirectionZ;

		simd::vec4_f32 tnear;
		simd::vec4_f32 tfar;
	};
	return false;
}

template<typename LeafObj>
inline void* PauseableBVH4<LeafObj>::encodeBVHConstructionLeafHandle(uint32_t handle)
{
	assert((handle & 0x7FFFFFFF) == handle);
	return reinterpret_cast<void*>(static_cast<uintptr_t>(handle | 0x80000000)); // Highest bit set to 1, indicating a leaf
}

template<typename LeafObj>
inline void* PauseableBVH4<LeafObj>::encodeBVHConstructionInnerNodeHandle(uint32_t handle)
{
	assert((handle & 0x7FFFFFFF) == handle);
	return reinterpret_cast<void*>(static_cast<uintptr_t>(handle & 0x7FFFFFFF)); // Highest bit set to 0, indicating an inner node
}

template<typename LeafObj>
inline std::tuple<bool, uint32_t> PauseableBVH4<LeafObj>::decodeBVHConstructionHandle(void* encodedHandlePtr)
{
	uint32_t encodedHandle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(encodedHandlePtr));
	bool isLeaf = encodedHandle & 0x80000000;
	uint32_t handle = encodedHandle & 0x7FFFFFFF;
	return { isLeaf, handle };
}

template <typename LeafObj>
inline void* PauseableBVH4<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
	auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
	auto[nodeHandle, nodePtr] = self->m_innerNodeAllocator.allocate();
	(void)nodePtr;
	
	return encodeBVHConstructionInnerNodeHandle(nodeHandle);
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
	assert(numChildren <= 4);

	auto [isLeaf, nodeHandle] = decodeBVHConstructionHandle(nodePtr);
	assert(!isLeaf);
	auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
	BVHNode& node = self->m_innerNodeAllocator.get(nodeHandle);

	uint32_t validMask = 0x0;
	uint32_t leafMask = 0x0;
	for (unsigned i = 0; i < numChildren; i++) {
		auto [isLeaf, childNodeHandle] = decodeBVHConstructionHandle(nodePtr);
		BVHNode& childNode = self->m_innerNodeAllocator.get(childNodeHandle);

		validMask |= 1 << i;
		if (isLeaf) {
			leafMask |= 1 << i;
		} else {
			childNode.parentHandle = nodeHandle;
		}

		node.childrenHandles[i] = childNodeHandle;
	}
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
	assert(numChildren <= 4);

	auto[isLeaf, nodeHandle] = decodeBVHConstructionHandle(nodePtr);
	assert(!isLeaf);
	auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
	BVHNode& node = self->m_innerNodeAllocator.get(nodeHandle);

	std::array<float, 4> minX, minY, minZ, maxX, maxY, maxZ;
	for (unsigned i = 0; i < numChildren; i++) {
		const RTCBounds* childBounds = bounds[i];
		minX[i] = childBounds->lower_x;
		minY[i] = childBounds->lower_y;
		minZ[i] = childBounds->lower_z;
		maxX[i] = childBounds->upper_x;
		maxY[i] = childBounds->upper_y;
		maxZ[i] = childBounds->upper_z;
	}
	for (unsigned i = numChildren; i < 4; i++) {
		minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
	}

	node.minX.load(minX);
	node.minY.load(minY);
	node.minZ.load(minZ);
	node.maxX.load(maxX);
	node.maxY.load(maxY);
	node.maxZ.load(maxZ);
}

template <typename LeafObj>
inline void* PauseableBVH4<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
	assert(numPrims == 1);

	// Allocate node
	auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
	auto[nodeHandle, nodePtr] = self->m_leafAllocator.allocate();
	*nodePtr = self->m_tmpConstructionLeafs[prims[0].primID];
	return encodeBVHConstructionLeafHandle(nodeHandle);
}

}
