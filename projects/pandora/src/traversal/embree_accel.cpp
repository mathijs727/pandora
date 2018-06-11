#include "pandora/traversal/embree_accel.h"
#include "embree3/rtcore_ray.h"
#include "pandora/geometry/scene.h"
#include "pandora/geometry/sphere.h"
#include "pandora/geometry/triangle.h"
#include <gsl/span>
#include <iostream>

namespace pandora {

EmbreeAccel::EmbreeAccel(const Scene& scene)
{
    m_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_device, embreeErrorFunc, nullptr);

    m_scene = rtcNewScene(m_device);
    rtcSetSceneBuildQuality(m_scene, RTC_BUILD_QUALITY_HIGH);

    for (const Shape* shape : scene.getShapes()) {
        if (auto triangleMesh = dynamic_cast<const TriangleMesh*>(shape)) {
            addTriangleMesh(*triangleMesh);
        } else if (auto sphere = dynamic_cast<const Sphere*>(shape)) {
            addSphere(*sphere);
        }
    }

    rtcCommitScene(m_scene);
}

EmbreeAccel::~EmbreeAccel()
{
    rtcReleaseScene(m_scene);
    rtcReleaseDevice(m_device);
}

template <unsigned N>
void convertRays(gsl::span<Ray> rays, RTCRayHitN* embreeRayHits)
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
void convertIntersections(RTCScene scene, RTCRayHitN* embreeRayHits, gsl::span<IntersectionData> intersections)
{
    RTCHitN* embreeHits = RTCRayHitN_HitN(embreeRayHits, N);
    for (unsigned i = 0; i < N; i++) {
        unsigned geomID = RTCHitN_geomID(embreeHits, N, i);
        if ( RTCHitN_geomID(embreeHits, N, i) == RTC_INVALID_GEOMETRY_ID) { // No hit
            intersections[i].objectHit = nullptr;
            return;
        }

        intersections[i].objectHit = reinterpret_cast<const Shape*>(rtcGetGeometryUserData(rtcGetGeometry(scene, geomID)));

        intersections[i].uv.x = RTCHitN_u(embreeHits, N, i);
        intersections[i].uv.y = RTCHitN_v(embreeHits, N, i);

        intersections[i].geometricNormal.x = RTCHitN_Ng_x(embreeHits, N, i);
        intersections[i].geometricNormal.y = RTCHitN_Ng_y(embreeHits, N, i);
        intersections[i].geometricNormal.z = RTCHitN_Ng_z(embreeHits, N, i);
    }
}

void EmbreeAccel::intersect(Ray& ray, IntersectionData& intersectionData)
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit embreeRayHit;
    convertRays<1>(gsl::make_span(&ray, 1), reinterpret_cast<RTCRayHitN*>(&embreeRayHit));

    rtcIntersect1(m_scene, &context, &embreeRayHit);

    convertIntersections<1>(m_scene, reinterpret_cast<RTCRayHitN*>(&embreeRayHit), gsl::make_span(&intersectionData, 1));
}

void EmbreeAccel::intersect(gsl::span<Ray> rays, gsl::span<IntersectionData> intersectionData)
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

void EmbreeAccel::addTriangleMesh(const TriangleMesh& triangleMesh)
{
    auto indices = triangleMesh.getIndices();
    auto positions = triangleMesh.getPositions();

    RTCGeometry mesh = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);
    auto indexBuffer = reinterpret_cast<glm::ivec3*>(rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(glm::ivec3), indices.size()));
    std::copy(indices.begin(), indices.end(), indexBuffer);

    auto vertexBuffer = reinterpret_cast<glm::vec3*>(rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(glm::vec3), positions.size()));
    std::copy(positions.begin(), positions.end(), vertexBuffer);

    rtcSetGeometryUserData(mesh, const_cast<TriangleMesh*>(&triangleMesh));

    rtcCommitGeometry(mesh);
    unsigned geomID = rtcAttachGeometry(m_scene, mesh);
    //rtcReleaseGeometry(mesh);


    /*unsigned geomID = rtcNewTriangleMesh2(m_scene, RTC_GEOMETRY_STATIC, indices.size(), positions.size());
	auto indexBuffer = reinterpret_cast<glm::ivec3*>(rtcMapBuffer(m_scene, geomID, RTC_INDEX_BUFFER));
	std::copy(indices.begin(), indices.end(), indexBuffer);
	rtcUnmapBuffer(m_scene, geomID, RTC_INDEX_BUFFER);

	struct EmbreeVertex {
		EmbreeVertex(const glm::vec3& v) : x(v.x), y(v.y), z(v.z), a(0.0f) { }
		float x, y, z, a;
	};
	auto vertexBuffer = reinterpret_cast<EmbreeVertex*>(rtcMapBuffer(m_scene, geomID, RTC_VERTEX_BUFFER));
	std::copy(positions.begin(), positions.end(), vertexBuffer);
	rtcUnmapBuffer(m_scene, geomID, RTC_VERTEX_BUFFER);

	rtcSetUserData(m_scene, geomID, const_cast<TriangleMesh*>(&triangleMesh));*/
}

void EmbreeAccel::addSphere(const Sphere& sphere)
{
    // NOT IMPLEMENTED YET
    std::cout << "Sphere not implemented yet for Embree accelerator" << std::endl;
}

void EmbreeAccel::embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
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

}
