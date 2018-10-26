#include "pandora/utility/error_handling.h"
#include <mutex>

namespace pandora {

static RTCBVH WiVeBVH8Build8_embreeBVH()
{
    static std::mutex m;
    std::scoped_lock<std::mutex> l(m);

    const auto embreeErrorFunc = [](void* userPtr, const RTCError code, const char* str) {
        switch (code) {
        case RTC_ERROR_NONE:
            std::cout << "RTC_ERROR_NONE";
            break;
        case RTC_ERROR_UNKNOWN:
            std::cout << "RTC_ERROR_UNKNOWN";
            break;
        case RTC_ERROR_INVALID_ARGUMENT:
            std::cout << "RTC_ERROR_INVALID_ARGUMENT";
            break;
        case RTC_ERROR_INVALID_OPERATION:
            std::cout << "RTC_ERROR_INVALID_OPERATION";
            break;
        case RTC_ERROR_OUT_OF_MEMORY:
            std::cout << "RTC_ERROR_OUT_OF_MEMORY";
            break;
        case RTC_ERROR_UNSUPPORTED_CPU:
            std::cout << "RTC_ERROR_UNSUPPORTED_CPU";
            break;
        case RTC_ERROR_CANCELLED:
            std::cout << "RTC_ERROR_CANCELLED";
            break;
        }

        std::cout << ": " << str << std::endl;
    };

    static RTCDevice device = nullptr;
    if (!device) {
        device = rtcNewDevice(nullptr);
        rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);
    }

    return rtcNewBVH(device);
}

template <typename LeafObj>
inline WiVeBVH8Build8<LeafObj>::WiVeBVH8Build8(gsl::span<LeafObj> objects)
    : WiVeBVH8<LeafObj>(objects.size())
{
    std::cout << "Reserving leaf objects" << std::endl;
    // Move the leaf objects
    std::cout << "Moving " << objects.size() << " leaf objects" << std::endl;
    this->m_leafObjects.reserve(objects.size());
    for (auto& object : objects) {
        this->m_leafObjects.emplace_back(std::move(object)); 
    }
    this->m_leafObjects.shrink_to_fit();

    std::cout << "Allocating primitives array" << std::endl;
    std::vector<RTCBuildPrimitive> embreePrimitives;
    embreePrimitives.reserve(static_cast<size_t>(objects.size()));

    ALWAYS_ASSERT(objects.size() < std::numeric_limits<unsigned>::max());

    std::cout << "Creating embree primitive array" << std::endl;
    for (uint64_t leafID = 0; leafID < static_cast<uint64_t>(this->m_leafObjects.size()); leafID++) {
        auto bounds = this->m_leafObjects[leafID].getBounds();// NOTE: use the local objects and not the original "objects" array (because its contents has been moved)

        RTCBuildPrimitive primitive;
        primitive.lower_x = bounds.min.x;
        primitive.lower_y = bounds.min.y;
        primitive.lower_z = bounds.min.z;
        primitive.upper_x = bounds.max.x;
        primitive.upper_y = bounds.max.y;
        primitive.upper_z = bounds.max.z;
        primitive.primID = leafID;
        primitive.geomID = 0;
        embreePrimitives.push_back(primitive);
    }

    std::cout << "Build Wive8BVH8 for " << objects.size() << " primitives" << std::endl;
    commit(embreePrimitives, objects);
    std::cout << "Build finished" << std::endl;
}

template <typename LeafObj>
inline void WiVeBVH8Build8<LeafObj>::commit(gsl::span<RTCBuildPrimitive> embreePrims, gsl::span<LeafObj> objects)
{
    /*RTCDevice device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);
    std::cout << "create embree BVH class" << std::endl;
    RTCBVH bvh = rtcNewBVH(device);*/
    RTCBVH bvh = WiVeBVH8Build8_embreeBVH(); // Prevent crashes when creating & deleting large numbers of Embree devices

    RTCBuildArguments arguments = rtcDefaultBuildArguments();
    arguments.byteSize = sizeof(arguments);
    arguments.buildFlags = RTC_BUILD_FLAG_NONE;
    arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
    arguments.maxBranchingFactor = 8;
    arguments.minLeafSize = 1;
    arguments.maxLeafSize = 4;
    arguments.bvh = bvh;
    arguments.primitives = embreePrims.data();
    arguments.primitiveCount = embreePrims.size();
    arguments.primitiveArrayCapacity = embreePrims.size();
    arguments.createNode = innerNodeCreate;
    arguments.setNodeChildren = innerNodeSetChildren;
    arguments.setNodeBounds = innerNodeSetBounds;
    arguments.createLeaf = leafCreate;
    arguments.userPtr = this;

    this->m_compressedRootHandle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rtcBuildBVH(&arguments)));

    // Releases Embree memory (including the temporary BVH)
    rtcReleaseBVH(bvh);
    //rtcReleaseDevice(device);

    // Shrink to fit BVH allocators
    this->m_innerNodeAllocator.compact();
    this->m_leafIndexAllocator.compact();

    //this->testBVH();
}

template <typename LeafObj>
inline void* WiVeBVH8Build8<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 8);

    // Allocate node
    auto* self = reinterpret_cast<WiVeBVH8Build8<LeafObj>*>(userPtr);
    auto [nodeHandle, nodePtr] = self->m_innerNodeAllocator.allocate();
    (void)nodePtr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(WiVeBVH8<LeafObj>::compressHandleInner(nodeHandle)));
}

template <typename LeafObj>
inline void WiVeBVH8Build8<LeafObj>::innerNodeSetChildren(void* p, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 8);

    uint32_t nodeHandle = WiVeBVH8<LeafObj>::decompressNodeHandle(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)));
    auto* self = reinterpret_cast<WiVeBVH8Build8<LeafObj>*>(userPtr);
    typename WiVeBVH8<LeafObj>::BVHNode& node = self->m_innerNodeAllocator.get(nodeHandle);

    std::array<uint32_t, 8> children;
    for (unsigned childID = 0; childID < numChildren; childID++) {
        children[childID] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(childPtr[childID]));
    }
    for (unsigned childID = numChildren; childID < 8; childID++) {
        children[childID] = WiVeBVH8<LeafObj>::compressHandleEmpty();
    }
    node.children.load(children);
}

template <typename LeafObj>
inline void WiVeBVH8Build8<LeafObj>::innerNodeSetBounds(void* p, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 8);

    uint32_t nodeHandle = WiVeBVH8<LeafObj>::decompressNodeHandle(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)));
    auto* self = reinterpret_cast<WiVeBVH8Build8<LeafObj>*>(userPtr);
    typename WiVeBVH8<LeafObj>::BVHNode& node = self->m_innerNodeAllocator.get(nodeHandle);

    std::array<float, 8> minX, minY, minZ, maxX, maxY, maxZ;
    std::array<uint32_t, 8> permutationOffsets;
    for (unsigned childID = 0; childID < numChildren; childID++) {
        const RTCBounds* childBounds = bounds[childID];
        minX[childID] = childBounds->lower_x;
        minY[childID] = childBounds->lower_y;
        minZ[childID] = childBounds->lower_z;
        maxX[childID] = childBounds->upper_x;
        maxY[childID] = childBounds->upper_y;
        maxZ[childID] = childBounds->upper_z;
    }
    for (unsigned i = numChildren; i < 8; i++) {
        minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
    }

    // Create permutations
    std::fill(std::begin(permutationOffsets), std::end(permutationOffsets), 0);
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                glm::vec3 direction(x, y, z);

                std::array<uint32_t, 8> indices = { 0, 1, 2, 3, 4, 5, 6, 7 };
                std::array<float, 8> distances;
                std::transform(std::begin(indices), std::end(indices), std::begin(distances), [&](uint32_t i) -> float {
                    if (i >= numChildren)
                        return std::numeric_limits<float>::max();

                    // Calculate the bounding box corner opposite to the direction vector
                    const RTCBounds* childBounds = bounds[i];
                    glm::vec3 extremePoint(
                        x == -1 ? childBounds->lower_x : childBounds->upper_x,
                        y == -1 ? childBounds->lower_y : childBounds->upper_y,
                        z == -1 ? childBounds->lower_z : childBounds->upper_z);
                    return glm::dot(extremePoint, direction);
                });
                // Sort bounding boxes back-to-front as seen from the normal plane of the direction vector
                std::sort(std::begin(indices), std::end(indices), [&](uint32_t a, uint32_t b) -> bool {
                    return distances[a] > distances[b];
                });

                uint32_t shiftAmount = WiVeBVH8<LeafObj>::signShiftAmount(x > 0, y > 0, z > 0);
                for (int i = 0; i < 8; i++)
                    permutationOffsets[i] |= indices[i] << shiftAmount;
            }
        }
    }

    node.minX.load(minX);
    node.minY.load(minY);
    node.minZ.load(minZ);
    node.maxX.load(maxX);
    node.maxY.load(maxY);
    node.maxZ.load(maxZ);
    node.permutationOffsets.load(permutationOffsets);
}

template <typename LeafObj>
inline void* WiVeBVH8Build8<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims > 0 && numPrims <= 4);

    // Allocate node
    auto* self = reinterpret_cast<WiVeBVH8Build8<LeafObj>*>(userPtr);
    auto [nodeHandle, nodePtr] = self->m_leafIndexAllocator.allocateNInitF((unsigned)numPrims, [&](int i) {
        return prims[i].primID;
    });
    /*auto[nodeHandle, nodePtr] = self->m_leafNodeAllocator->allocate();
	for (size_t i = 0; i < numPrims; i++) {
		nodePtr->leafObjectIDs[i] = prims[i].geomID;
		nodePtr->primitiveIDs[i] = prims[i].primID;
	}
	for (size_t i = numPrims; i < 4; i++) {
		nodePtr->leafObjectIDs[i] = WiVeBVH8<LeafObj>::emptyHandle;
		nodePtr->primitiveIDs[i] = WiVeBVH8<LeafObj>::emptyHandle;
	}*/
    return reinterpret_cast<void*>(static_cast<uintptr_t>(WiVeBVH8<LeafObj>::compressHandleLeaf(nodeHandle, (uint32_t)numPrims)));
}
}
