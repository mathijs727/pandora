#include "glm/gtc/type_ptr.hpp"
#include <iostream>

namespace pandora {

template <unsigned N>
void convertRays(gsl::span<const Ray> rays, RTCRayHitN* embreeRayHits)
{
    RTCRayN* embreeRays = RTCRayHitN_RayN(embreeRayHits, N);
    RTCHitN* embreeHits = RTCRayHitN_HitN(embreeRayHits, N);
    for (unsigned i = 0; i < N; i++) {
        RTCRayN_org_x(embreeRays, N, i) = rays[i].origin.x;
        RTCRayN_org_y(embreeRays, N, i) = rays[i].origin.y;
        RTCRayN_org_z(embreeRays, N, i) = rays[i].origin.z;
        RTCRayN_dir_x(embreeRays, N, i) = rays[i].direction.x;
        RTCRayN_dir_y(embreeRays, N, i) = rays[i].direction.y;
        RTCRayN_dir_z(embreeRays, N, i) = rays[i].direction.z;
        RTCRayN_tnear(embreeRays, N, i) = rays[i].tnear;
        RTCRayN_tfar(embreeRays, N, i) = rays[i].tfar;

        RTCHitN_geomID(embreeHits, N, i) = RTC_INVALID_GEOMETRY_ID;
        //RTCHitN_instID(embreeHits, N, i, 0) = RTC_INVALID_GEOMETRY_ID;
    }
}

template <unsigned N>
void convertIntersections(RTCScene scene, RTCRayHitN* embreeRayHits, gsl::span<SurfaceInteraction> intersections)
{
    RTCHitN* embreeHits = RTCRayHitN_HitN(embreeRayHits, N);
    RTCRayN* embreeRays = RTCRayHitN_RayN(embreeRayHits, N);
    for (unsigned i = 0; i < N; i++) {

        unsigned geomID = RTCHitN_geomID(embreeHits, N, i);
        if (RTCHitN_geomID(embreeHits, N, i) == RTC_INVALID_GEOMETRY_ID) { // No hit
            intersections[i].sceneObject = nullptr;
            glm::vec3 direction = glm::vec3(RTCRayN_dir_x(embreeRays, N, i), RTCRayN_dir_y(embreeRays, N, i), RTCRayN_dir_z(embreeRays, N, i));
            intersections[i].wo = -direction;
            return;
        }

        RTCGeometry geometry = rtcGetGeometry(scene, geomID);
        const auto* sceneObject = reinterpret_cast<const SceneObject*>(rtcGetGeometryUserData(geometry));
        const auto* mesh = sceneObject->mesh.get();
        unsigned primID = RTCHitN_primID(embreeHits, N, i);
        glm::vec2 hitUV = glm::vec2(RTCHitN_u(embreeHits, N, i), RTCHitN_v(embreeHits, N, i));
        intersections[i] = mesh->partialFillSurfaceInteraction(primID, hitUV);
        intersections[i].sceneObject = sceneObject;

        glm::vec3 direction = glm::vec3(RTCRayN_dir_x(embreeRays, N, i), RTCRayN_dir_y(embreeRays, N, i), RTCRayN_dir_z(embreeRays, N, i));
        intersections[i].wo = -direction;

        /*glm::vec3 origin = glm::vec3(RTCRayN_org_x(embreeRays, N, i), RTCRayN_org_y(embreeRays, N, i), RTCRayN_org_z(embreeRays, N, i));
        float t = RTCRayN_tfar(embreeRays, N, i);
        intersections[i].position = origin + t * direction;

        glm::vec3 geometricNormal = glm::vec3(RTCHitN_Ng_x(embreeHits, N, i), RTCHitN_Ng_y(embreeHits, N, i), RTCHitN_Ng_z(embreeHits, N, i));
        intersections[i].normal = glm::normalize(geometricNormal);

        glm::vec3 shadingNormal;
        RTCInterpolateArguments arguments = {};
        arguments.geometry = geometry;
        arguments.primID = RTCHitN_primID(embreeHits, N, i);
        arguments.u = RTCHitN_u(embreeHits, N, i);
        arguments.v = RTCHitN_v(embreeHits, N, i);
        arguments.bufferType = RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE;
        arguments.bufferSlot = 0;
        arguments.valueCount = 3;
        arguments.P = glm::value_ptr(shadingNormal);
        rtcInterpolate(&arguments);
        intersections[i].shading.normal = glm::normalize(shadingNormal);

        if (sceneObject->mesh->getUVCoords()) {
            RTCInterpolateArguments arguments = {};
            arguments.geometry = geometry;
            arguments.primID = RTCHitN_primID(embreeHits, N, i);
            arguments.u = RTCHitN_u(embreeHits, N, i);
            arguments.v = RTCHitN_v(embreeHits, N, i);
            arguments.bufferType = RTC_BUFFER_TYPE_FACE;
            arguments.bufferSlot = 1;
            arguments.valueCount = 2;
            arguments.P = glm::value_ptr(intersections[i].uv);
            rtcInterpolate(&arguments);
        } else {
            intersections[i].uv.x = RTCHitN_u(embreeHits, N, i);
            intersections[i].uv.y = RTCHitN_v(embreeHits, N, i);
        }*/
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

template <typename UserState>
inline EmbreeAccel<UserState>::EmbreeAccel(gsl::span<const SceneObject> sceneObjects, ShadingCallback shadingCallback)
    : m_shadingCallback(shadingCallback)
{
    m_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_device, embreeErrorFunc, nullptr);

    m_scene = rtcNewScene(m_device);
    rtcSetSceneBuildQuality(m_scene, RTC_BUILD_QUALITY_HIGH);

    for (const auto& sceneObject : sceneObjects) {
        addSceneObject(sceneObject);
    }

    rtcCommitScene(m_scene);
}

template <typename UserState>
inline EmbreeAccel<UserState>::~EmbreeAccel()
{
    rtcReleaseScene(m_scene);
    rtcReleaseDevice(m_device);
}

template <typename UserState>
inline void EmbreeAccel<UserState>::placeIntersectRequests(gsl::span<const UserState> perRayUserData, gsl::span<const Ray> rays)
{
    assert(perRayUserData.size() == rays.size());
    for (int i = 0; i < perRayUserData.size(); i++) {
        SurfaceInteraction intersectionData;
        intersect(rays[i], intersectionData);
        m_shadingCallback(rays[i], intersectionData, perRayUserData[i], nullptr);
    }
}

template <typename UserState>
inline void EmbreeAccel<UserState>::placeIntersectRequests(gsl::span<const UserState> perRayUserData, gsl::span<const Ray> rays, const EmbreeInsertHandle& insertHandle)
{
    placeIntersectRequests(perRayUserData, rays);
}

template <typename UserState>
inline void EmbreeAccel<UserState>::intersect(const Ray& ray, SurfaceInteraction& intersectionData) const
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit embreeRayHit;
    convertRays<1>(gsl::make_span(&ray, 1), reinterpret_cast<RTCRayHitN*>(&embreeRayHit));

    rtcIntersect1(m_scene, &context, &embreeRayHit);

    convertIntersections<1>(m_scene, reinterpret_cast<RTCRayHitN*>(&embreeRayHit), gsl::make_span(&intersectionData, 1));
}

template <typename UserState>
inline void EmbreeAccel<UserState>::intersectPacket(gsl::span<const Ray, 8> rays, gsl::span<SurfaceInteraction> intersectionData) const
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit8 embreeRayHitPacket;
    convertRays<1>(rays, reinterpret_cast<RTCRayHitN*>(&embreeRayHitPacket));

    std::array<int32_t, 8> validMasks;
    std::fill(validMasks.begin(), validMasks.end(), 0xFFFFFFFF);
    rtcIntersect8(validMasks.data(), m_scene, &context, &embreeRayHitPacket);

    convertIntersections<8>(m_scene, reinterpret_cast<RTCRayHitN*>(&embreeRayHitPacket), intersectionData);
}

template <typename UserState>
inline void EmbreeAccel<UserState>::addSceneObject(const SceneObject& sceneObject)
{
    const auto& triangleMesh = *sceneObject.mesh;
    auto triangles = triangleMesh.getTriangles();
    auto positions = triangleMesh.getPositions();

    RTCGeometry mesh = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);

    auto indexBuffer = reinterpret_cast<glm::ivec3*>(rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(glm::ivec3), triangles.size()));
    std::copy(triangles.begin(), triangles.end(), indexBuffer);

    auto vertexBuffer = reinterpret_cast<glm::vec3*>(rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(glm::vec3), positions.size()));
    std::copy(positions.begin(), positions.end(), vertexBuffer);

    rtcSetGeometryUserData(mesh, const_cast<SceneObject*>(&sceneObject));

    rtcCommitGeometry(mesh);
    //unsigned geomID = rtcAttachGeometry(m_scene, mesh);
    rtcAttachGeometry(m_scene, mesh);
}

}
