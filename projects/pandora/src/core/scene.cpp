#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"
#include <embree3/rtcore.h>
#include <iostream>
#include <numeric>
#include <tbb/concurrent_vector.h>
#include <unordered_set>
#include <filesystem>
#include <string>
#include <thread>

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

gsl::span<const std::unique_ptr<InCoreSceneObject>> Scene::getInCoreSceneObjects() const
{
    return m_inCoreSceneObjects;
}

gsl::span<const std::unique_ptr<OOCSceneObject>> Scene::getOOCSceneObjects() const
{
    return m_oocSceneObjects;
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

std::vector<std::vector<const OOCSceneObject*>> groupSceneObjects(
    unsigned primitivesPerGroup,
    gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects)
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
        virtual std::pair<std::vector<const OOCSceneObject*>, std::unordered_set<const OOCSceneObject*>> group(
            uint32_t minPrimsPerGroup, std::vector<std::vector<const OOCSceneObject*>>& out) const = 0;
    };

    struct BVHInnerNode : public BVHNode {
        virtual std::pair<std::vector<const OOCSceneObject*>, std::unordered_set<const OOCSceneObject*>> group(
            uint32_t minPrimsPerGroup, std::vector<std::vector<const OOCSceneObject*>>& out) const
        {
            auto [leftObjects, leftUniqueAndBaseObjects] = leftChild->group(minPrimsPerGroup, out);
            auto [rightObjects, rightUniqueAndBaseObjects] = rightChild->group(minPrimsPerGroup, out);
            if (!leftObjects.empty() && rightObjects.empty()) {
                return { std::move(leftObjects), std::move(leftUniqueAndBaseObjects) };
            } else if (leftObjects.empty() && !rightObjects.empty()) {
                return { std::move(rightObjects), std::move(rightUniqueAndBaseObjects) };
            } else if (!leftObjects.empty() && !rightObjects.empty()) {
                std::vector<const OOCSceneObject*> objects = std::move(leftObjects);
                objects.insert(std::end(objects), std::begin(rightObjects), std::end(rightObjects));

                std::unordered_set<const OOCSceneObject*> uniqueAndBaseObjects = std::move(leftUniqueAndBaseObjects);
                uniqueAndBaseObjects.insert(std::begin(rightUniqueAndBaseObjects), std::end(rightUniqueAndBaseObjects));

#ifdef _MSC_VER
                uint32_t numUniqueAndBasePrims = std::transform_reduce(
                    std::begin(uniqueAndBaseObjects),
                    std::end(uniqueAndBaseObjects),
                    0u,
                    [](uint32_t l, uint32_t r) {
                        return l + r;
                    },
                    [](const OOCSceneObject* object) {
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
        virtual std::pair<std::vector<const OOCSceneObject*>, std::unordered_set<const OOCSceneObject*>> group(
            uint32_t minPrimsPerGroup, std::vector<std::vector<const OOCSceneObject*>>& out) const
        {
            if (sceneObject->numPrimitives() > minPrimsPerGroup) {
                std::cout << "Add single scene object because prims " << sceneObject->numPrimitives() << " > " << minPrimsPerGroup << std::endl;
                out.emplace_back(std::vector<const OOCSceneObject*> { sceneObject });
                return { {}, {} };
            }

            if (const auto* instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject)) {
                return { { sceneObject }, { instancedSceneObject->getBaseObject() } };
            } else {
                return { { sceneObject }, { sceneObject } };
            }
        }
        const OOCSceneObject* sceneObject;
    };

    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    embreeBuildPrimitives.reserve(sceneObjects.size());
    uint32_t sceneObjectID = 0;
    for (const auto& sceneObject : sceneObjects) {
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
    arguments.userPtr = &sceneObjects;
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

        auto sceneObjects = *reinterpret_cast<gsl::span<const std::unique_ptr<OOCSceneObject>>*>(userPtr);

        auto* mem = rtcThreadLocalAlloc(alloc, sizeof(BVHLeafNode), 8);
        auto nodePtr = new (mem) BVHLeafNode();
        nodePtr->sceneObject = sceneObjects[prims[0].primID].get();
        return nodePtr;
    };

    const auto* rootNode = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    std::vector<std::vector<const OOCSceneObject*>> ret;
    auto [leftOverObjects, leftOverUniqueAndBaseObject] = rootNode->group(primitivesPerGroup, ret);
    if (!leftOverObjects.empty()) {
        ret.emplace_back(std::move(leftOverObjects));
    }

    for (const auto& group : ret) {
        ALWAYS_ASSERT(group.size() > 0);
    }

    // Free Embree memory (including memory allocated with rtcThreadLocalAlloc
    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);
    return ret;
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

/*struct SplitSceneBVHNode {
    virtual std::vector<unsigned> outputSceneObjects(
        unsigned minPrimsPerPart,
        std::vector<std::vector<unsigned>>& partPrims) const = 0;
};

struct SplitSceneBVHInnerNode : public SplitSceneBVHNode {
    virtual std::vector<unsigned> outputSceneObjects(
        unsigned minPrimsPerPart,
        std::vector<std::vector<unsigned>>& partPrims) const override final
    {
        auto primsLeft = m_left->outputSceneObjects(minPrimsPerPart, partPrims);
        auto primsRight = m_right->outputSceneObjects(minPrimsPerPart, partPrims);
        auto numPrimsLeft = primsLeft.size();
        auto numPrimsRight = primsRight.size();

        if (numPrimsLeft == 0 && numPrimsRight == 0)
            return {};

        if (numPrimsLeft == 0 && numPrimsRight != 0) {
            if (numPrimsRight > minPrimsPerPart) {
                partPrims.push_back(std::move(primsRight));
                return {};
            } else {
                return primsRight;
            }
        } else if (numPrimsLeft != 0 && numPrimsRight == 0) {
            if (numPrimsLeft > minPrimsPerPart) {
                partPrims.push_back(std::move(primsLeft));
                return {};
            } else {
                return primsLeft;
            }
        } else {
            // Merge the two arrays and check whether the result is large enough to become an output part
            primsLeft.insert(std::end(primsLeft), std::begin(primsRight), std::end(primsRight));
            if (numPrimsLeft + numPrimsRight > minPrimsPerPart) {
                partPrims.push_back(std::move(primsLeft));
                return {};
            } else {
                return primsLeft;
            }
        }
    }

    SplitSceneBVHNode* m_left;
    SplitSceneBVHNode* m_right;
};

struct SplitSceneBVHLeafNode : public SplitSceneBVHNode {
    virtual std::vector<unsigned> outputSceneObjects(
        unsigned minPrimsPerPart,
        std::vector<std::vector<unsigned>>& partPrims) const override final
    {
        return m_prims;
    }

    std::vector<unsigned> m_prims;
};

void splitLargeSceneObjects(unsigned maxPrimitivesPerSceneObject)
{
    std::vector<std::unique_ptr<SceneObject>> splitSceneObjects;
    for (size_t j = 0; j < m_sceneObjects.size(); j++) {
        auto& sceneObject = m_sceneObjects[j]; // Dont use range based for loop while the vector is mutated

        unsigned size = sceneObject->getMeshRef().numTriangles();
        if (size < maxPrimitivesPerSceneObject) {
            splitSceneObjects.push_back(std::move(sceneObject));
            continue;
        }

        // Use Embree to geometrically split the mesh (as opposed to splitting based on primitive index)
        std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
        embreeBuildPrimitives.reserve(size);
        for (unsigned i = 0; i < size; i++) {
            auto bounds = sceneObject->getMeshRef().getPrimitiveBounds(i);

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
        arguments.maxBranchingFactor = 2;
        arguments.minLeafSize = maxPrimitivesPerSceneObject / 8;
        arguments.maxLeafSize = maxPrimitivesPerSceneObject / 8;
        arguments.bvh = bvh;
        arguments.primitives = embreeBuildPrimitives.data();
        arguments.primitiveCount = embreeBuildPrimitives.size();
        arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
        arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr) -> void* {
            auto* mem = rtcThreadLocalAlloc(alloc, sizeof(SplitSceneBVHInnerNode), 8);
            return new (mem) SplitSceneBVHInnerNode();
        };
        arguments.setNodeChildren = [](void* voidNodePtr, void** childPtr, unsigned numChildren, void* userPtr) {
            ALWAYS_ASSERT(numChildren == 2);
            auto* nodePtr = reinterpret_cast<SplitSceneBVHInnerNode*>(voidNodePtr);

            nodePtr->m_left = reinterpret_cast<SplitSceneBVHNode*>(childPtr[0]);
            nodePtr->m_right = reinterpret_cast<SplitSceneBVHNode*>(childPtr[1]);
        };
        arguments.setNodeBounds = [](void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr) {};
        arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
            auto* mem = rtcThreadLocalAlloc(alloc, sizeof(SplitSceneBVHLeafNode), 8);
            auto nodePtr = new (mem) SplitSceneBVHLeafNode();
            nodePtr->m_prims = std::vector<unsigned>(numPrims);
            for (size_t i = 0; i < numPrims; i++)
                nodePtr->m_prims[i] = prims[i].primID;
            return nodePtr;
        };

        const auto* rootNode = reinterpret_cast<SplitSceneBVHNode*>(rtcBuildBVH(&arguments));
        std::vector<std::vector<unsigned>> partPrimitives;
        auto leftOverPrims = rootNode->outputSceneObjects(maxPrimitivesPerSceneObject / 2, partPrimitives);
        if (leftOverPrims.size() < maxPrimitivesPerSceneObject / 8) {
            partPrimitives.back().insert(std::end(partPrimitives.back()), std::begin(leftOverPrims), std::end(leftOverPrims));
        } else {
            partPrimitives.push_back(leftOverPrims);
        }
        //ALWAYS_ASSERT(leftOverPrims.size() > 0);
        //partPrimitives.push_back(leftOverPrims);

        rtcReleaseBVH(bvh);
        rtcReleaseDevice(device);

        auto material = sceneObject->getMaterial();
        for (const auto& primitiveIDs : partPrimitives) {
            ALWAYS_ASSERT(primitiveIDs.size() > 10);
            auto mesh = std::make_shared<const TriangleMesh>(sceneObject->getMeshRef().subMesh(primitiveIDs));
            splitSceneObjects.push_back(std::make_unique<SceneObject>(mesh, material));
        }
    }
    m_sceneObjects = std::move(splitSceneObjects);

    std::cout << "NUM SCENE OBJECTS AFTER SPLIT: " << m_sceneObjects.size() << std::endl;
}*/
}

/*SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material)
    : m_mesh(mesh)
    , m_material(material)
{
}

SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted)
    : m_mesh(mesh)
    , m_material(material)
{
    m_areaLightPerPrimitive.reserve(m_mesh->numTriangles());
    for (unsigned i = 0; i < mesh->numTriangles(); i++) {
        m_areaLightPerPrimitive.emplace_back(lightEmitted, 1, *mesh, i);
    }
}

SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const glm::mat4& instanceToWorldMatrix, const std::shared_ptr<const Material>& material)
    : m_mesh(mesh)
    , m_transform(instanceToWorldMatrix)
    , m_material(material)
{
}

SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const glm::mat4& instanceToWorldMatrix, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted)
    : m_mesh(mesh)
    , m_transform(instanceToWorldMatrix)
    , m_material(material)
{
    m_areaLightPerPrimitive.reserve(m_mesh->numTriangles());
    for (unsigned i = 0; i < mesh->numTriangles(); i++) {
        m_areaLightPerPrimitive.emplace_back(lightEmitted, 1, *mesh, i);
    }
}

const AreaLight* SceneObject::getAreaLight(unsigned primID) const
{
    if (m_areaLightPerPrimitive.empty())
        return nullptr;
    else
        return &m_areaLightPerPrimitive[primID];
}

std::optional<gsl::span<const AreaLight>> SceneObject::getAreaLights() const
{
    if (m_areaLightPerPrimitive.empty())
        return {};
    else
        return m_areaLightPerPrimitive;
}*/
