#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/graphics_core/transform.h"
#include "pandora/traversal/acceleration_structure.h"
#include "stream/task_graph.h"
#include <embree3/rtcore.h>
#include <glm/gtc/type_ptr.hpp>
#include <gsl/span>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pandora {

class EmbreeAccelerationStructureBuilder;

template <typename HitRayState, typename AnyHitRayState>
class EmbreeAccelerationStructure : public AccelerationStructure<HitRayState, AnyHitRayState> {
public:
    ~EmbreeAccelerationStructure();

    void intersect(const Ray& ray, const HitRayState& state) const;
    void intersectAny(const Ray& ray, const AnyHitRayState& state) const;

    std::optional<SurfaceInteraction> intersectDebug(const Ray& ray) const;

private:
    void intersectKernel(gsl::span<const std::tuple<Ray, HitRayState>> data, std::pmr::memory_resource* pMemoryResource);
    void intersectAnyKernel(gsl::span<const std::tuple<Ray, AnyHitRayState>> data, std::pmr::memory_resource* pMemoryResource);

private:
    friend class EmbreeAccelerationStructureBuilder;
    EmbreeAccelerationStructure(
        RTCDevice embreeDevice, RTCScene embreeScene,
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
        tasking::TaskGraph* pTaskGraph);

private:
    RTCDevice m_embreeDevice;
    RTCScene m_embreeScene;

    tasking::TaskGraph* m_pTaskGraph;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_intersectTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_intersectAnyTask;

    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> m_onHitTask;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_onMissTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyHitTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyMissTask;
};

class EmbreeAccelerationStructureBuilder {
public:
    EmbreeAccelerationStructureBuilder(const Scene& scene, tasking::TaskGraph* pTaskGraph);

    template <typename HitRayState, typename AnyHitRayState>
    EmbreeAccelerationStructure<HitRayState, AnyHitRayState> build(
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    RTCScene buildRecurse(const SceneNode* pNode);
    static void verifyInstanceDepth(const SceneNode* pNode, int depth = 0);

private:
    RTCDevice m_embreeDevice;
    RTCScene m_embreeScene;
    std::unordered_map<const SceneNode*, RTCScene> m_sceneCache;

    tasking::TaskGraph* m_pTaskGraph;
};

template <typename HitRayState, typename AnyHitRayState>
inline std::optional<SurfaceInteraction> EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectDebug(const Ray& ray) const
{

    RTCRayHit embreeRayHit;
    embreeRayHit.ray.org_x = ray.origin.x;
    embreeRayHit.ray.org_y = ray.origin.y;
    embreeRayHit.ray.org_z = ray.origin.z;
    embreeRayHit.ray.dir_x = ray.direction.x;
    embreeRayHit.ray.dir_y = ray.direction.y;
    embreeRayHit.ray.dir_z = ray.direction.z;

    embreeRayHit.ray.tnear = ray.tnear;
    embreeRayHit.ray.tfar = ray.tfar;

    embreeRayHit.ray.time = 0.0f;
    embreeRayHit.ray.mask = 0xFFFFFFFF;
    embreeRayHit.ray.id = 0;
    embreeRayHit.ray.flags = 0;
    embreeRayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    // Embree sets to 0 when invalid?
    for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++) {
        embreeRayHit.hit.instID[i] = RTC_INVALID_GEOMETRY_ID;
    }
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    rtcIntersect1(m_embreeScene, &context, &embreeRayHit);

    if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        std::optional<glm::mat4> optLocalToWorldMatrix;
        const SceneObject* pSceneObject { nullptr };

        if (embreeRayHit.hit.instID[0] == RTC_INVALID_GEOMETRY_ID) {
            pSceneObject = reinterpret_cast<const SceneObject*>(
                rtcGetGeometryUserData(rtcGetGeometry(m_embreeScene, embreeRayHit.hit.geomID)));
        } else {
            glm::mat4 accumulatedTransform { 1.0f };
            RTCScene scene = m_embreeScene;
            for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++) {
                unsigned geomID = embreeRayHit.hit.instID[i];
                if (geomID == RTC_INVALID_GEOMETRY_ID)
                    break;

                RTCGeometry geometry = rtcGetGeometry(scene, geomID);

                glm::mat4 localTransform;
                rtcGetGeometryTransform(geometry, 0.0f, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, glm::value_ptr(localTransform));
                accumulatedTransform *= localTransform;

                scene = reinterpret_cast<RTCScene>(rtcGetGeometryUserData(geometry));
            }

            optLocalToWorldMatrix = accumulatedTransform;
            pSceneObject = reinterpret_cast<const SceneObject*>(
                rtcGetGeometryUserData(rtcGetGeometry(scene, embreeRayHit.hit.geomID)));
        }

        RayHit hit;
        hit.geometricNormal = { embreeRayHit.hit.Ng_x, embreeRayHit.hit.Ng_y, embreeRayHit.hit.Ng_z };
        hit.geometricNormal = glm::normalize(hit.geometricNormal); // Normal from Emrbee is already in object space.
        hit.geometricUV = { embreeRayHit.hit.u, embreeRayHit.hit.v };
        hit.primitiveID = embreeRayHit.hit.primID;

        const auto* pShape = pSceneObject->pShape.get();
        SurfaceInteraction si;
        if (optLocalToWorldMatrix) {
            // Transform from world space to shape local space.
            Transform transform { *optLocalToWorldMatrix };
            Ray localRay = transform.transformToLocal(ray);
            // Hit is already in object space...
            //hit = transform.transformToLocal(hit);

            // Fill surface interaction in local space
            si = pShape->fillSurfaceInteraction(localRay, hit);

            // Transform surface interaction back to world space
            si = transform.transformToWorld(si);
        } else {
            // Tell surface interaction which the shape was hit.
            si = pShape->fillSurfaceInteraction(ray, hit);
        }

        // Flip the normal if it is facing away from the ray.
        if (glm::dot(si.normal, -ray.direction) < 0)
            si.normal = -si.normal;
        if (glm::dot(si.shading.normal, -ray.direction) < 0)
            si.shading.normal = -si.shading.normal;

        si.pSceneObject = pSceneObject;
        si.localToWorld = optLocalToWorldMatrix;
        return si;
    } else {
        return {};
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState> EmbreeAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    return EmbreeAccelerationStructure(m_embreeDevice, m_embreeScene, hitTask, missTask, anyHitTask, anyMissTask, m_pTaskGraph);
}

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::EmbreeAccelerationStructure(
    RTCDevice embreeDevice, RTCScene embreeScene,
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
    tasking::TaskGraph* pTaskGraph)
    : m_embreeDevice(embreeDevice)
    , m_embreeScene(embreeScene)
    , m_pTaskGraph(pTaskGraph)
    , m_intersectTask(
          pTaskGraph->addTask<std::tuple<Ray, HitRayState>>(
              "EmbreeAccelerationStructure::intersect",
              [this](auto data, auto* pMemRes) { intersectKernel(data, pMemRes); }))
    , m_intersectAnyTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyHitRayState>>(
              "EmbreeAccelerationStructure::intersectAny",
              [this](auto data, auto* pMemRes) { intersectAnyKernel(data, pMemRes); }))
    , m_onHitTask(hitTask)
    , m_onMissTask(missTask)
    , m_onAnyHitTask(anyHitTask)
    , m_onAnyMissTask(anyMissTask)
{
}

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::~EmbreeAccelerationStructure()
{
    rtcReleaseScene(m_embreeScene);
    rtcReleaseDevice(m_embreeDevice);
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersect(const Ray& ray, const HitRayState& state) const
{
    m_pTaskGraph->enqueue(m_intersectTask, std::tuple { ray, state });
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectAny(const Ray& ray, const AnyHitRayState& state) const
{
    m_pTaskGraph->enqueue(m_intersectAnyTask, std::tuple { ray, state });
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectKernel(
    gsl::span<const std::tuple<Ray, HitRayState>> data, std::pmr::memory_resource* pMemoryResource)
{
    for (auto [ray, state] : data) {
        RTCRayHit embreeRayHit;
        embreeRayHit.ray.org_x = ray.origin.x;
        embreeRayHit.ray.org_y = ray.origin.y;
        embreeRayHit.ray.org_z = ray.origin.z;
        embreeRayHit.ray.dir_x = ray.direction.x;
        embreeRayHit.ray.dir_y = ray.direction.y;
        embreeRayHit.ray.dir_z = ray.direction.z;

        embreeRayHit.ray.tnear = ray.tnear;
        embreeRayHit.ray.tfar = ray.tfar;

        embreeRayHit.ray.time = 0.0f;
        embreeRayHit.ray.mask = 0xFFFFFFFF;
        embreeRayHit.ray.id = 0;
        embreeRayHit.ray.flags = 0;
        embreeRayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
        for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++)
            embreeRayHit.hit.instID[i] = RTC_INVALID_GEOMETRY_ID;

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcIntersect1(m_embreeScene, &context, &embreeRayHit);

        if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            std::optional<glm::mat4> optLocalToWorldMatrix;
            const SceneObject* pSceneObject { nullptr };

            if (embreeRayHit.hit.instID[0] == RTC_INVALID_GEOMETRY_ID) {
                pSceneObject = reinterpret_cast<const SceneObject*>(
                    rtcGetGeometryUserData(rtcGetGeometry(m_embreeScene, embreeRayHit.hit.geomID)));
            } else {
                glm::mat4 accumulatedTransform { 1.0f };
                RTCScene scene = m_embreeScene;
                for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++) {
                    unsigned geomID = embreeRayHit.hit.instID[i];
                    if (geomID == RTC_INVALID_GEOMETRY_ID)
                        break;

                    RTCGeometry geometry = rtcGetGeometry(scene, geomID);

                    glm::mat4 localTransform;
                    rtcGetGeometryTransform(geometry, 0.0f, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, glm::value_ptr(localTransform));
                    accumulatedTransform *= localTransform;

                    scene = reinterpret_cast<RTCScene>(rtcGetGeometryUserData(geometry));
                }

                optLocalToWorldMatrix = accumulatedTransform;
                pSceneObject = reinterpret_cast<const SceneObject*>(
                    rtcGetGeometryUserData(rtcGetGeometry(scene, embreeRayHit.hit.geomID)));
            }

            RayHit hit;
            hit.geometricNormal = { embreeRayHit.hit.Ng_x, embreeRayHit.hit.Ng_y, embreeRayHit.hit.Ng_z };
            hit.geometricNormal = glm::normalize(hit.geometricNormal); // Normal from Emrbee is already in object space.
            hit.geometricUV = { embreeRayHit.hit.u, embreeRayHit.hit.v };
            hit.primitiveID = embreeRayHit.hit.primID;

            const auto* pShape = pSceneObject->pShape.get();
            SurfaceInteraction si;
            if (optLocalToWorldMatrix) {
                // Transform from world space to shape local space.
                Transform transform { *optLocalToWorldMatrix };
                Ray localRay = transform.transformToLocal(ray);
                // Hit is already in object space...
                //hit = transform.transformToLocal(hit);

                // Fill surface interaction in local space
                si = pShape->fillSurfaceInteraction(localRay, hit);

                // Transform surface interaction back to world space
                si = transform.transformToWorld(si);
            } else {
                // Tell surface interaction which the shape was hit.
                si = pShape->fillSurfaceInteraction(ray, hit);
            }
            // Flip the normal if it is facing away from the ray.
            if (glm::dot(si.normal, -ray.direction) < 0)
                si.normal = -si.normal;
            if (glm::dot(si.shading.normal, -ray.direction) < 0)
                si.shading.normal = -si.shading.normal;

            si.pSceneObject = pSceneObject;
            //si.localToWorld = optLocalToWorldMatrix;
            m_pTaskGraph->enqueue(m_onHitTask, std::tuple { ray, si, state });
        } else {
            m_pTaskGraph->enqueue(m_onMissTask, std::tuple { ray, state });
        }
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectAnyKernel(
    gsl::span<const std::tuple<Ray, AnyHitRayState>> data, std::pmr::memory_resource* pMemoryResource)
{
    for (auto [ray, state] : data) {
        RTCRay embreeRay;
        embreeRay.org_x = ray.origin.x;
        embreeRay.org_y = ray.origin.y;
        embreeRay.org_z = ray.origin.z;
        embreeRay.dir_x = ray.direction.x;
        embreeRay.dir_y = ray.direction.y;
        embreeRay.dir_z = ray.direction.z;

        embreeRay.tnear = ray.tnear;
        embreeRay.tfar = ray.tfar;

        embreeRay.time = 0.0f;
        embreeRay.mask = 0xFFFFFFFF;
        embreeRay.id = 0;
        embreeRay.flags = 0;

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcOccluded1(m_embreeScene, &context, &embreeRay);

        constexpr float negativeInfinity = -std::numeric_limits<float>::infinity();
        if (embreeRay.tfar == negativeInfinity) {
            m_pTaskGraph->enqueue(m_onAnyHitTask, std::tuple { ray, state });
        } else {
            m_pTaskGraph->enqueue(m_onAnyMissTask, std::tuple { ray, state });
        }
    }
}
}