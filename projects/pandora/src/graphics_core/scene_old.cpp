#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/light.h"
#include "pandora/lights/area_light.h"
#include "pandora/utility/error_handling.h"
#include <embree3/rtcore.h>
#include <filesystem>
#include <numeric>
#include <string>
#include <unordered_set>

using namespace std::string_literals;

namespace pandora {

void Scene::addSceneObject(std::unique_ptr<SceneObject>&& sceneObject)
{
    for (unsigned primitiveID = 0; primitiveID < sceneObject->numPrimitives(); primitiveID++) {
        if (const auto* light = sceneObject->getPrimitiveAreaLight(primitiveID); light) {
            m_lights.push_back(light);
        }
    }
    m_bounds.extend(sceneObject->worldBounds());
    m_sceneObjects.emplace_back(std::move(sceneObject));
}

void Scene::addInfiniteLight(const std::shared_ptr<InfiniteLight>& light)
{
    m_lights.push_back(light.get());
    m_infiniteLights.push_back(light.get());

    m_lightOwningPointers.push_back(light);
}

std::vector<const SceneObject*> Scene::getSceneObjects() const
{
    std::vector<const SceneObject*> ret;
    ret.reserve(m_sceneObjects.size());
    std::transform(
        std::begin(m_sceneObjects),
        std::end(m_sceneObjects),
        std::back_inserter(ret),
        [](const auto& uniquePtr) { return uniquePtr.get(); });
    return ret;
}

std::span<const Light* const> Scene::getLights() const
{
    return m_lights;
}

std::span<const InfiniteLight* const> Scene::getInfiniteLights() const
{
    return m_infiniteLights;
}

/*void Scene::splitLargeInCoreSceneObjects(unsigned approximatePrimsPerObject)
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

    RTCDevice device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);

    int outFileCount = 0;
    std::vector<std::unique_ptr<InCoreSceneObject>> outSceneObjects;
    for (auto& sceneObject : m_inCoreSceneObjects) {
        if (sceneObject->numPrimitives() < approximatePrimsPerObject) {
            outSceneObjects.push_back(std::move(sceneObject));
            continue;
        }

        auto* geometricSceneObject = dynamic_cast<InCoreGeometricSceneObject*>(sceneObject.get());
        if (!geometricSceneObject) {
            outSceneObjects.push_back(std::move(sceneObject));
            continue; // Instanced scene objects are not split
        }

        // NOTE: if we want to support splitting area lights than the old lights should be removed from the
        // lights array. We should than also make sure that the new lights are always added in the same order
        // to prevent race conditions.
        ALWAYS_ASSERT(sceneObject->areaLights().empty());

        std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
        embreeBuildPrimitives.reserve(sceneObject->numPrimitives());
        for (unsigned primID = 0; primID < sceneObject->numPrimitives(); primID++) {
            auto bounds = sceneObject->worldBoundsPrimitive(primID);

            RTCBuildPrimitive primitive;
            primitive.lower_x = bounds.min.x;
            primitive.lower_y = bounds.min.y;
            primitive.lower_z = bounds.min.z;
            primitive.upper_x = bounds.max.x;
            primitive.upper_y = bounds.max.y;
            primitive.upper_z = bounds.max.z;
            primitive.geomID = 0;
            primitive.primID = primID;
            embreeBuildPrimitives.push_back(primitive);
        }

        // Build the BVH using the Embree BVH builder API
        RTCBVH bvh = rtcNewBVH(device);

        tbb::concurrent_vector<std::span<unsigned>> primitiveGroups;
        RTCBuildArguments arguments = rtcDefaultBuildArguments();
        arguments.byteSize = sizeof(arguments);
        arguments.buildFlags = RTC_BUILD_FLAG_NONE;
        arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
        arguments.maxBranchingFactor = 2;
        arguments.minLeafSize = approximatePrimsPerObject; // Stop splitting when number of prims is below minLeafSize
        arguments.maxLeafSize = approximatePrimsPerObject; // This is a hard constraint (always split when number of prims is larger than maxLeafSize)
        arguments.bvh = bvh;
        arguments.primitives = embreeBuildPrimitives.data();
        arguments.primitiveCount = embreeBuildPrimitives.size();
        arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
        arguments.userPtr = &primitiveGroups;
        arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr) -> void* {
            return nullptr;
        };
        arguments.setNodeChildren = [](void* voidNodePtr, void** childPtr, unsigned numChildren, void* userPtr) {};
        arguments.setNodeBounds = [](void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr) {};
        arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
            auto* primsMem = rtcThreadLocalAlloc(alloc, numPrims * sizeof(unsigned), 8);
            unsigned* primitiveIDs = reinterpret_cast<unsigned*>(primsMem);

            for (size_t i = 0; i < numPrims; i++) {
                primitiveIDs[i] = prims[i].primID;
            }

            auto primitiveGroups = reinterpret_cast<tbb::concurrent_vector<std::span<unsigned>>*>(userPtr);
            primitiveGroups->push_back(std::span(primitiveIDs, numPrims));
            return nullptr;
        };

        rtcBuildBVH(&arguments); // Fill primitiveGroups vector

        for (auto primitiveGroup : primitiveGroups) {
            auto splitSceneObject = geometricSceneObject->geometricSplit(primitiveGroup);
            outSceneObjects.emplace_back(new InCoreGeometricSceneObject(std::move(splitSceneObject)));
        }

        // Free Embree memory (including memory allocated with rtcThreadLocalAlloc)
        rtcReleaseBVH(bvh);
    }

    // Free Embree memory
    rtcReleaseDevice(device);

    m_inCoreSceneObjects = std::move(outSceneObjects);
}

void Scene::splitLargeOOCSceneObjects(unsigned approximatePrimsPerObject)
{
    std::filesystem::path outFolder("./split_scene_objects/");
    if (!std::filesystem::exists(outFolder)) {
        std::filesystem::create_directories(outFolder);
    }

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

    RTCDevice device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);

    std::cout << "Splitting large scene objects" << std::endl;
    int outFileCount = 0;
    std::vector<std::unique_ptr<OOCSceneObject>> outSceneObjects;
    for (auto& sceneObject : m_oocSceneObjects) {
        if (sceneObject->numPrimitives() < approximatePrimsPerObject) {
            outSceneObjects.push_back(std::move(sceneObject));
            continue;
        }

        auto* geometricSceneObject = dynamic_cast<OOCGeometricSceneObject*>(sceneObject.get());
        if (!geometricSceneObject) {
            outSceneObjects.push_back(std::move(sceneObject));
            continue; // Instanced scene objects are not split
        }

        std::cout << "Split object: " << outFileCount << std::endl;

        // NOTE: if we want to support splitting area lights than the old lights should be removed from the
        // lights array. We should than also make sure that the new lights are always added in the same order
        // to prevent race conditions.
        ALWAYS_ASSERT(sceneObject->getMaterialBlocking()->areaLights().empty());

        auto sceneObjectGeometry = sceneObject->getGeometryBlocking();

        std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
        embreeBuildPrimitives.reserve(sceneObject->numPrimitives());
        for (unsigned primID = 0; primID < sceneObject->numPrimitives(); primID++) {
            auto bounds = sceneObjectGeometry->worldBoundsPrimitive(primID);

            RTCBuildPrimitive primitive;
            primitive.lower_x = bounds.min.x;
            primitive.lower_y = bounds.min.y;
            primitive.lower_z = bounds.min.z;
            primitive.upper_x = bounds.max.x;
            primitive.upper_y = bounds.max.y;
            primitive.upper_z = bounds.max.z;
            primitive.geomID = 0;
            primitive.primID = primID;
            embreeBuildPrimitives.push_back(primitive);
        }

        // Build the BVH using the Embree BVH builder API
        RTCBVH bvh = rtcNewBVH(device);

        tbb::concurrent_vector<std::vector<unsigned>> primitiveGroups;

        RTCBuildArguments arguments = rtcDefaultBuildArguments();
        arguments.byteSize = sizeof(arguments);
        arguments.buildFlags = RTC_BUILD_FLAG_NONE;
        arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
        arguments.maxBranchingFactor = 2;
        arguments.minLeafSize = approximatePrimsPerObject; // Stop splitting when number of prims is below minLeafSize
        arguments.maxLeafSize = approximatePrimsPerObject; // This is a hard constraint (always split when number of prims is larger than maxLeafSize)
        arguments.bvh = bvh;
        arguments.primitives = embreeBuildPrimitives.data();
        arguments.primitiveCount = embreeBuildPrimitives.size();
        arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
        arguments.userPtr = &primitiveGroups;
        arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr) -> void* {
            return nullptr;
        };
        arguments.setNodeChildren = [](void* voidNodePtr, void** childPtr, unsigned numChildren, void* userPtr) {};
        arguments.setNodeBounds = [](void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr) {};
        arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
            std::vector<unsigned> primitiveIDs(numPrims);
            for (size_t i = 0; i < numPrims; i++) {
                primitiveIDs[i] = prims[i].primID;
            }

            auto primitiveGroups = reinterpret_cast<tbb::concurrent_vector<std::vector<unsigned>>*>(userPtr);
            primitiveGroups->emplace_back(std::move(primitiveIDs));
            return nullptr;
        };

        rtcBuildBVH(&arguments); // Fill primitiveGroups vector

        for (auto primitiveGroup : primitiveGroups) {
            std::filesystem::path outFile = outFolder / ("submesh"s + std::to_string(outFileCount++) + ".bin"s);

            auto splitSceneObject = geometricSceneObject->geometricSplit(m_geometryCache.get(), outFile, primitiveGroup);
            outSceneObjects.emplace_back(new OOCGeometricSceneObject(std::move(splitSceneObject)));
        }

        // Free Embree memory (including memory allocated with rtcThreadLocalAlloc)
        rtcReleaseBVH(bvh);
    }

    // Free Embree memory
    rtcReleaseDevice(device);

    m_oocSceneObjects = std::move(outSceneObjects);
}*/

std::vector<std::vector<const SceneObject*>> groupSceneObjects(std::span<const std::unique_ptr<SceneObject>> objects, unsigned uniquePrimsPerGroup)
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
        virtual std::vector<const SceneObject*> group(
            unsigned minPrimsPerGroup, std::vector<std::vector<const SceneObject*>>& out) const = 0;
    };

    struct BVHInnerNode : public BVHNode {
        std::vector<const SceneObject*> group(unsigned minPrimsPerGroup, std::vector<std::vector<const SceneObject*>>& out) const override final
        {
            auto leftObjects = leftChild->group(minPrimsPerGroup, out);
            auto rightObjects = rightChild->group(minPrimsPerGroup, out);
            if (!leftObjects.empty() && rightObjects.empty()) {
                return std::move(leftObjects);
            } else if (leftObjects.empty() && !rightObjects.empty()) {
                return std::move(rightObjects);
            } else if (!leftObjects.empty() && !rightObjects.empty()) {
                std::vector<const SceneObject*> objects = std::move(leftObjects);
                objects.insert(std::end(objects), std::begin(rightObjects), std::end(rightObjects));

#ifdef _MSC_VER
                unsigned numPrimitives = std::transform_reduce(
                    std::begin(objects),
                    std::end(objects),
                    0u,
                    [](unsigned l, unsigned r) {
                        return l + r;
                    },
                    [](const SceneObject* object) {
                        return object->numPrimitives();
                    });
#else // GCC 8.2.1 stdlibc++ does not support the standardization of parallelism TS
                unsigned numPrimitives = 0;
                for (const auto* object : objects)
                    numPrimitives += object->numPrimitives();
#endif

                if (numPrimitives >= minPrimsPerGroup) {
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
        std::vector<const SceneObject*> group(unsigned minPrimsPerGroup, std::vector<std::vector<const SceneObject*>>& out) const override final
        {
            if (sceneObject->numPrimitives() > minPrimsPerGroup) {
                std::cout << "Add single scene object because prims " << sceneObject->numPrimitives() << " > " << minPrimsPerGroup << std::endl;
                out.emplace_back(std::vector<const SceneObject*> { sceneObject });
                return {};
            }

            return { sceneObject };
        }
        const SceneObject* sceneObject;
    };

    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    embreeBuildPrimitives.reserve(objects.size());
    uint32_t sceneObjectID = 0;
    for (const auto& sceneObject : objects) {
        auto bounds = sceneObject->worldBounds();

        RTCBuildPrimitive primitive;
        primitive.lower_x = bounds.min.x;
        primitive.lower_y = bounds.min.y;
        primitive.lower_z = bounds.min.z;
        primitive.upper_x = bounds.max.x;
        primitive.upper_y = bounds.max.y;
        primitive.upper_z = bounds.max.z;
        primitive.geomID = 0;
        primitive.primID = sceneObjectID++;
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
    arguments.maxLeafSize = 1;
    arguments.maxDepth = 48;
    arguments.bvh = bvh;
    arguments.primitives = embreeBuildPrimitives.data();
    arguments.primitiveCount = embreeBuildPrimitives.size();
    arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
    arguments.userPtr = (void*)&objects;
    arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr) -> void* {
        auto* mem = rtcThreadLocalAlloc(alloc, sizeof(BVHInnerNode), 8);
        return new (mem) BVHInnerNode();
    };
    arguments.setNodeChildren = [](void* voidNodePtr, void** childPtr, unsigned numChildren, void* userPtr) {
        ALWAYS_ASSERT(numChildren == 2);
        auto* nodePtr = reinterpret_cast<BVHInnerNode*>(voidNodePtr);

        nodePtr->leftChild = reinterpret_cast<BVHNode*>(childPtr[0]);
        nodePtr->rightChild = reinterpret_cast<BVHNode*>(childPtr[1]);
    };
    arguments.setNodeBounds = [](void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr) {};
    arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
        ALWAYS_ASSERT(numPrims == 1);

        const auto& sceneObjects = *reinterpret_cast<std::span<std::unique_ptr<SceneObject>>*>(userPtr);

        auto* mem = rtcThreadLocalAlloc(alloc, sizeof(BVHLeafNode), 8);
        auto nodePtr = new (mem) BVHLeafNode();
        nodePtr->sceneObject = sceneObjects[prims[0].primID].get();
        return nodePtr;
    };

    const auto* rootNode = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    std::vector<std::vector<const SceneObject*>> ret;
    auto leftOverObjects = rootNode->group(uniquePrimsPerGroup, ret);
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
