#include "..\..\include\pandora\core\scene.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include <embree3/rtcore.h>
#include <iostream>
#include <tbb/concurrent_vector.h>

//static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);

namespace pandora {

GeometricSceneObject::GeometricSceneObject(
    const std::shared_ptr<const TriangleMesh>& mesh,
    const std::shared_ptr<const Material>& material)
    : m_mesh(mesh)
    , m_material(material)
    , m_areaLightPerPrimitive()
{
}

GeometricSceneObject::GeometricSceneObject(
    const std::shared_ptr<const TriangleMesh>& mesh,
    const std::shared_ptr<const Material>& material,
    const Spectrum& lightEmitted)
    : m_mesh(mesh)
    , m_material(material)
    , m_areaLightPerPrimitive()
{
    for (unsigned primitiveID = 0; m_mesh->numTriangles(); primitiveID++)
        m_areaLightPerPrimitive.push_back(AreaLight(lightEmitted, 1, *mesh, primitiveID));
}

Bounds GeometricSceneObject::worldBounds() const
{
    return m_mesh->getBounds();
}

Bounds GeometricSceneObject::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_mesh->getPrimitiveBounds(primitiveID);
}

unsigned GeometricSceneObject::numPrimitives() const
{
    return m_mesh->numTriangles();
}

bool GeometricSceneObject::intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const
{
    return m_mesh->intersectPrimitive(ray, rayHit, primitiveID);
}

SurfaceInteraction GeometricSceneObject::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{
    return m_mesh->fillSurfaceInteraction(ray, rayHit);
}

const AreaLight* GeometricSceneObject::getPrimitiveAreaLight(unsigned primitiveID) const
{
    if (m_areaLightPerPrimitive.empty())
        return nullptr;
    else
        return &m_areaLightPerPrimitive[primitiveID];
}

const Material* GeometricSceneObject::getMaterial() const
{
    return m_material.get();
}

void GeometricSceneObject::computeScatteringFunctions(
    SurfaceInteraction& si,
    ShadingMemoryArena& memoryArena,
    TransportMode mode,
    bool allowMultipleLobes) const
{
    m_material->computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}


gsl::span<const std::unique_ptr<SceneObject>> Scene::getSceneObjects() const
{
    return m_sceneObjects;
}

gsl::span<const Light* const> Scene::getLights() const
{
    return m_lights;
}

gsl::span<const Light* const> Scene::getInfiniteLights() const
{
    return m_infiniteLights;
}

void Scene::addSceneObject(std::unique_ptr<SceneObject>&& sceneObject)
{
    for (unsigned primitiveID = 0; primitiveID < sceneObject->numPrimitives(); primitiveID++) {
        if (const auto* light = sceneObject->getPrimitiveAreaLight(primitiveID); light)
        {
            m_lights.push_back(light);
        }
    }
    m_sceneObjects.emplace_back(std::move(sceneObject));
}

void Scene::addInfiniteLight(const std::shared_ptr<Light>& light)
{
    m_lights.push_back(light.get());
    m_infiniteLights.push_back(light.get());

    m_lightOwningPointers.push_back(light);
}

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
}

struct SplitSceneBVHNode {
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

void Scene::splitLargeSceneObjects(unsigned maxPrimitivesPerSceneObject)
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
}



}

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
{
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
}*/
