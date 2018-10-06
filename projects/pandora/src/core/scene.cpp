#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"
#include <embree3/rtcore.h>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <tbb/concurrent_vector.h>
#include <thread>
#include <unordered_set>

using namespace std::string_literals;

namespace pandora {

Scene::Scene(size_t geometryCacheSize)
    : m_geometryCache(std::make_unique<FifoCache<TriangleMesh>>(geometryCacheSize, 2 * std::thread::hardware_concurrency()))
{
}

void Scene::addSceneObject(std::unique_ptr<InCoreSceneObject>&& sceneObject)
{
    for (unsigned primitiveID = 0; primitiveID < sceneObject->numPrimitives(); primitiveID++) {
        if (const auto* light = sceneObject->getPrimitiveAreaLight(primitiveID); light) {
            m_lights.push_back(light);
        }
    }
    m_inCoreSceneObjects.emplace_back(std::move(sceneObject));
}

void Scene::addSceneObject(std::unique_ptr<OOCSceneObject>&& sceneObject)
{
    //std::cout << "(scene.cpp) addSceneObject cannot access area lights yet" << std::endl;
    auto material = sceneObject->getMaterialBlocking();
    for (const auto& light : material->areaLights()) {
        m_lights.push_back(&light);
    }
    m_oocSceneObjects.emplace_back(std::move(sceneObject));
}

void Scene::addInfiniteLight(const std::shared_ptr<Light>& light)
{
    m_lights.push_back(light.get());
    m_infiniteLights.push_back(light.get());

    m_lightOwningPointers.push_back(light);
}

std::vector<const InCoreSceneObject*> Scene::getInCoreSceneObjects() const
{
    std::vector<const InCoreSceneObject*> ret;
    ret.reserve(m_inCoreSceneObjects.size());
    std::transform(
        std::begin(m_inCoreSceneObjects),
        std::end(m_inCoreSceneObjects),
        std::back_inserter(ret),
        [](const auto& uniquePtr) { return uniquePtr.get(); });
    return ret;
}

std::vector<const OOCSceneObject*> Scene::getOOCSceneObjects() const
{
    std::vector<const OOCSceneObject*> ret;
    ret.reserve(m_oocSceneObjects.size());
    std::transform(
        std::begin(m_oocSceneObjects),
        std::end(m_oocSceneObjects),
        std::back_inserter(ret),
        [](const auto& uniquePtr) { return uniquePtr.get(); });
    return ret;
}

gsl::span<const Light* const> Scene::getLights() const
{
    return m_lights;
}

gsl::span<const Light* const> Scene::getInfiniteLights() const
{
    return m_infiniteLights;
}

const FifoCache<TriangleMesh>* Scene::geometryCache() const
{
    return m_geometryCache.get();
}

FifoCache<TriangleMesh>* Scene::geometryCache()
{
    return m_geometryCache.get();
}

void Scene::splitLargeInCoreSceneObjects(unsigned approximatePrimsPerObject)
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

        tbb::concurrent_vector<gsl::span<unsigned>> primitiveGroups;
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

            auto primitiveGroups = reinterpret_cast<tbb::concurrent_vector<gsl::span<unsigned>>*>(userPtr);
            primitiveGroups->push_back(gsl::make_span(primitiveIDs, numPrims));
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

        tbb::concurrent_vector<gsl::span<unsigned>> primitiveGroups;
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

            auto primitiveGroups = reinterpret_cast<tbb::concurrent_vector<gsl::span<unsigned>>*>(userPtr);
            primitiveGroups->push_back(gsl::make_span(primitiveIDs, numPrims));
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
}

template <typename T, typename InstancedT>
std::vector<std::vector<const T*>> groupSceneObjects(gsl::span<const std::unique_ptr<T>> objects, unsigned uniquePrimsPerGroup)
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
        virtual std::pair<std::vector<const T*>, std::unordered_set<const T*>> group(
            unsigned minPrimsPerGroup, std::vector<std::vector<const T*>>& out) const = 0;
    };

    struct BVHInnerNode : public BVHNode {
        virtual std::pair<std::vector<const T*>, std::unordered_set<const T*>> group(
            unsigned minPrimsPerGroup, std::vector<std::vector<const T*>>& out) const
        {
            auto[leftObjects, leftUniqueAndBaseObjects] = leftChild->group(minPrimsPerGroup, out);
            auto[rightObjects, rightUniqueAndBaseObjects] = rightChild->group(minPrimsPerGroup, out);
            if (!leftObjects.empty() && rightObjects.empty()) {
                return { std::move(leftObjects), std::move(leftUniqueAndBaseObjects) };
            } else if (leftObjects.empty() && !rightObjects.empty()) {
                return { std::move(rightObjects), std::move(rightUniqueAndBaseObjects) };
            } else if (!leftObjects.empty() && !rightObjects.empty()) {
                std::vector<const T*> objects = std::move(leftObjects);
                objects.insert(std::end(objects), std::begin(rightObjects), std::end(rightObjects));

                std::unordered_set<const T*> uniqueAndBaseObjects = std::move(leftUniqueAndBaseObjects);
                uniqueAndBaseObjects.insert(std::begin(rightUniqueAndBaseObjects), std::end(rightUniqueAndBaseObjects));

#ifdef _MSC_VER
                unsigned numUniqueAndBasePrims = std::transform_reduce(
                    std::begin(uniqueAndBaseObjects),
                    std::end(uniqueAndBaseObjects),
                    0u,
                    [](unsigned l, unsigned r) {
                    return l + r;
                },
                    [](const T* object) {
                    return object->numPrimitives();
                });
#else // GCC 8.2.1 stdlib does not support the standardization of parallelism TS
                int32_t numUniqueAndBasePrims = 0;
                for (const auto* object : uniqueAndBaseObjects)
                    numUniqueAndBasePrims += object->numPrimitives();
#endif
                if (numUniqueAndBasePrims >= minPrimsPerGroup) {
                    out.emplace_back(std::move(objects));
                    return { {}, {} };
                } else {
                    return { std::move(objects), std::move(uniqueAndBaseObjects) };
                }
            } else {
                return { {}, {} };
            }
        }

        const BVHNode* leftChild;
        const BVHNode* rightChild;
    };

    struct BVHLeafNode : public BVHNode {
        virtual std::pair<std::vector<const T*>, std::unordered_set<const T*>> group(
            unsigned minPrimsPerGroup, std::vector<std::vector<const T*>>& out) const
        {
            if (sceneObject->numPrimitives() > minPrimsPerGroup) {
                std::cout << "Add single scene object because prims " << sceneObject->numPrimitives() << " > " << minPrimsPerGroup << std::endl;
                out.emplace_back(std::vector<const T*> { sceneObject });
                return { {}, {} };
            }

            if (const auto* instancedSceneObject = dynamic_cast<const InstancedT*>(sceneObject)) {
                return { { sceneObject }, { instancedSceneObject->getBaseObject() } };
            } else {
                return { { sceneObject }, { sceneObject } };
            }
        }
        const T* sceneObject;
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

        const auto& sceneObjects = *reinterpret_cast<gsl::span<std::unique_ptr<T>>*>(userPtr);

        auto* mem = rtcThreadLocalAlloc(alloc, sizeof(BVHLeafNode), 8);
        auto nodePtr = new (mem) BVHLeafNode();
        nodePtr->sceneObject = sceneObjects[prims[0].primID].get();
        return nodePtr;
    };

    const auto* rootNode = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    std::vector<std::vector<const T*>> ret;
    auto[leftOverObjects, leftOverUniqueAndBaseObject] = rootNode->group(uniquePrimsPerGroup, ret);
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


std::vector<std::vector<const OOCSceneObject*>> Scene::groupOOCSceneObjects(unsigned uniquePrimsPerGroup) const
{
    return groupSceneObjects<OOCSceneObject, OOCInstancedSceneObject>(m_oocSceneObjects, uniquePrimsPerGroup);
}

std::vector<std::vector<const InCoreSceneObject*>> Scene::groupInCoreSceneObjects(unsigned uniquePrimsPerGroup) const
{
    return groupSceneObjects<InCoreSceneObject, InCoreInstancedSceneObject>(m_inCoreSceneObjects, uniquePrimsPerGroup);
}

}