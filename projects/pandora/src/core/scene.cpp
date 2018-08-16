#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include <embree3/rtcore.h>
#include <iostream>
#include <tbb/concurrent_vector.h>

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);

namespace pandora {

SceneObject::SceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material)
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

void Scene::addSceneObject(std::unique_ptr<SceneObject>&& sceneObject)
{
    if (auto areaLights = sceneObject->getAreaLights(); areaLights) {
        for (const auto& light : *areaLights)
            m_lights.push_back(&light);
    }
    m_sceneObjects.emplace_back(std::move(sceneObject));
}

void Scene::addInfiniteLight(const std::shared_ptr<Light>& light)
{
    m_lights.push_back(light.get());
    m_infiniteLights.push_back(light.get());

    m_lightOwningPointers.push_back(light);
}

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
        arguments.minLeafSize = maxPrimitivesPerSceneObject / 2;
        arguments.maxLeafSize = maxPrimitivesPerSceneObject;
        arguments.bvh = bvh;
        arguments.primitives = embreeBuildPrimitives.data();
        arguments.primitiveCount = embreeBuildPrimitives.size();
        arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
        arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr) -> void* { return nullptr; };
        arguments.setNodeChildren = [](void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr) {};
        arguments.setNodeBounds = [](void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr) {};
        arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
            auto partPrimitives = reinterpret_cast<tbb::concurrent_vector<std::vector<unsigned>>*>(userPtr);

            std::vector<unsigned> primitiveIDs;
            for (size_t i = 0; i < numPrims; i++) {
                primitiveIDs.push_back(prims[i].primID);
            }
            partPrimitives->push_back(std::move(primitiveIDs));

            return nullptr;
        };

        tbb::concurrent_vector<std::vector<unsigned>> partPrimitives;
        arguments.userPtr = &partPrimitives;
        rtcBuildBVH(&arguments);
        rtcReleaseBVH(bvh);
        rtcReleaseDevice(device);

        auto material = sceneObject->getMaterial();
        for (const auto& primitiveIDs : partPrimitives) {
            auto mesh = std::make_shared<const TriangleMesh>(sceneObject->getMeshRef().subMesh(primitiveIDs));
            splitSceneObjects.push_back(std::make_unique<SceneObject>(mesh, material));
        }
    }
    m_sceneObjects = std::move(splitSceneObjects);
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
}
