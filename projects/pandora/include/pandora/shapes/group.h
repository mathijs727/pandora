#pragma once
#include <embree3/rtcore.h>
#include <span>
#include <iostream>
#include <tuple>
#include <vector>

namespace pandora {

template <typename T>
std::vector<std::vector<T>> geometricGroupObjects(std::span<const T> objects, unsigned objectsPerGroup)
{
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

    struct BVHNode {
        virtual std::vector<T> group(unsigned minObjectsPerGroup, std::vector<std::vector<T>>& out) const = 0;
    };

    struct BVHInnerNode : public BVHNode {
        virtual std::vector<T> group(unsigned minObjectsPerGroup, std::vector<std::vector<T>>& out) const
        {
            auto leftObjects = leftChild->group(minObjectsPerGroup, out);
            auto rightObjects = rightChild->group(minObjectsPerGroup, out);
            if (!leftObjects.empty() && rightObjects.empty()) {
                return std::move(leftObjects);
            } else if (leftObjects.empty() && !rightObjects.empty()) {
                return std::move(rightObjects);
            } else if (!leftObjects.empty() && !rightObjects.empty()) {
                std::vector<T> objects = std::move(leftObjects);
                objects.insert(std::end(objects), std::begin(rightObjects), std::end(rightObjects));

                if (objects.size() >= minObjectsPerGroup) {
                    out.emplace_back(std::move(objects));
                    return {};
                } else {
                    return std::move(objects);
                }
            } else {
                return {};
            }
        }

        const BVHNode* leftChild;
        const BVHNode* rightChild;
    };

    struct BVHLeafNode : public BVHNode {
        virtual std::vector<T> group(unsigned minObjectsPerGroup, std::vector<std::vector<T>>& out) const
        {
            std::vector<T> ret;
            ret.reserve(objects.size());
            ret.insert(std::end(ret), std::begin(objects), std::end(objects));
            return ret;
        }
        eastl::fixed_vector<T, 8> objects;
    };

    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    embreeBuildPrimitives.reserve(objects.size());
    uint32_t objectID = 0;
    for (const auto& object : objects) {
        auto bounds = object.worldBounds();

        RTCBuildPrimitive primitive;
        primitive.lower_x = bounds.min.x;
        primitive.lower_y = bounds.min.y;
        primitive.lower_z = bounds.min.z;
        primitive.upper_x = bounds.max.x;
        primitive.upper_y = bounds.max.y;
        primitive.upper_z = bounds.max.z;
        primitive.geomID = 0;
        primitive.primID = objectID++;
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
    arguments.maxBranchingFactor = 2;
    arguments.minLeafSize = 1;
    arguments.maxLeafSize = 8;
    arguments.bvh = bvh;
    arguments.primitives = embreeBuildPrimitives.data();
    arguments.primitiveCount = embreeBuildPrimitives.size();
    arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
    arguments.userPtr = (void*)&objects;
    arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr) -> void* {
        auto* mem = rtcThreadLocalAlloc(alloc, sizeof(BVHInnerNode), 8);
        new (mem) BVHInnerNode();
        return mem;
    };
    arguments.setNodeChildren = [](void* voidNodePtr, void** childPtr, unsigned numChildren, void* userPtr) {
        ALWAYS_ASSERT(numChildren == 2);
        auto* nodePtr = reinterpret_cast<BVHInnerNode*>(voidNodePtr);

        nodePtr->leftChild = reinterpret_cast<BVHNode*>(childPtr[0]);
        nodePtr->rightChild = reinterpret_cast<BVHNode*>(childPtr[1]);
    };
    arguments.setNodeBounds = [](void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr) {};
    arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
        ALWAYS_ASSERT(numPrims <= 8);

        const auto& objects = *reinterpret_cast<std::span<const T>*>(userPtr);

        auto* mem = rtcThreadLocalAlloc(alloc, sizeof(BVHLeafNode), 8);
        auto* node = new (mem) BVHLeafNode();
        for (size_t i = 0; i < numPrims; i++) {
            node->objects.push_back(objects[prims[i].primID]);
        }
        return mem;
    };

    const auto* rootNode = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    std::vector<std::vector<T>> ret;
    auto leftOverObjects = rootNode->group(objectsPerGroup, ret);
    if (!leftOverObjects.empty()) {
        ret.emplace_back(std::move(leftOverObjects));
    }

    for (const auto& group : ret) {
        ALWAYS_ASSERT(group.size() > 0);
    }

    // Free Embree memory (including memory allocated with rtcThreadLocalAlloc)
    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);
    return ret;
}

}
