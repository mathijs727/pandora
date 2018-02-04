#include "pandora/traversal/embree_accel.h"
#include "pandora/geometry/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/geometry/sphere.h"
#include <iostream>
#include <embree2/rtcore_ray.h>
#include <gsl/span>

namespace pandora {

EmbreeAccel::EmbreeAccel(const SceneView& scene)
{
	m_device = rtcNewDevice(nullptr);
	rtcDeviceSetErrorFunction2(m_device, embreeErrorFunc, nullptr);

	RTCSceneFlags sceneFlags = RTC_SCENE_STATIC | RTC_SCENE_HIGH_QUALITY;
	RTCAlgorithmFlags algorithmFlags = RTC_INTERSECT1;
	m_scene = rtcDeviceNewScene(m_device, sceneFlags, algorithmFlags);

	for (const Shape* shape : scene.getShapes()) {
		if (auto triangleMesh = dynamic_cast<const TriangleMesh*>(shape)) {
			addTriangleMesh(*triangleMesh);
		} else if (auto sphere = dynamic_cast<const Sphere*>(shape)) {
			addSphere(*sphere);
		}
	}

	rtcCommit(m_scene);
}

EmbreeAccel::~EmbreeAccel()
{
	rtcDeleteDevice(m_device);
}

template <size_t N>
void convertRays(gsl::span<Ray> rays, RTCRayN* embreeRays)
{
	for (size_t i = 0; i < N; i++) {
		RTCRayN_org_x(embreeRays, N, i) = rays[i].origin.x;
		RTCRayN_org_y(embreeRays, N, i) = rays[i].origin.y;
		RTCRayN_org_z(embreeRays, N, i) = rays[i].origin.z;
		RTCRayN_dir_x(embreeRays, N, i) = rays[i].direction.x;
		RTCRayN_dir_y(embreeRays, N, i) = rays[i].direction.y;
		RTCRayN_dir_z(embreeRays, N, i) = rays[i].direction.z;
		RTCRayN_tnear(embreeRays, N, i) = rays[i].tnear;
		RTCRayN_tfar(embreeRays, N, i) = rays[i].tfar;
		RTCRayN_geomID(embreeRays, N, i) = RTC_INVALID_GEOMETRY_ID;
	}
}

template <size_t N>
void convertIntersections(RTCScene scene, RTCRayN* embreeRays, gsl::span<IntersectionData> intersections)
{
	for (size_t i = 0; i < N; i++) {
		unsigned geomID = RTCRayN_geomID(embreeRays, N, i);
		if (geomID == RTC_INVALID_GEOMETRY_ID) {// No hit
			intersections[i].objectHit = nullptr;
			return;
		}

		intersections[i].objectHit = reinterpret_cast<const Shape*>(rtcGetUserData(scene, geomID));

		intersections[i].uv.x = RTCRayN_u(embreeRays, N, i);
		intersections[i].uv.y = RTCRayN_v(embreeRays, N, i);

		intersections[i].geometricNormal.x = RTCRayN_Ng_x(embreeRays, N, i);
		intersections[i].geometricNormal.y = RTCRayN_Ng_y(embreeRays, N, i);
		intersections[i].geometricNormal.z = RTCRayN_Ng_z(embreeRays, N, i);
	}
}

void EmbreeAccel::intersect(Ray& ray, IntersectionData& intersectionData)
{
	RTCRay embreeRay;
	convertRays<1>(gsl::make_span(&ray, 1), reinterpret_cast<RTCRayN*>(&embreeRay));

	rtcIntersect(m_scene, embreeRay);

	convertIntersections<1>(m_scene, reinterpret_cast<RTCRayN*>(&embreeRay), gsl::make_span(&intersectionData, 1));
}

void EmbreeAccel::intersect(gsl::span<Ray> rays, gsl::span<IntersectionData> intersectionData)
{
	RTCRay8 embreeRayPacket;
	convertRays<8>(rays, reinterpret_cast<RTCRayN*>(&embreeRayPacket));

	std::array<int32_t, 8> validMasks;
	std::fill(validMasks.begin(), validMasks.end(), 0xFFFFFFFF);
	rtcIntersect8(validMasks.data(), m_scene, embreeRayPacket);

	convertIntersections<8>(m_scene, reinterpret_cast<RTCRayN*>(&embreeRayPacket), intersectionData);
}

void EmbreeAccel::addTriangleMesh(const TriangleMesh& triangleMesh)
{
	auto indices = triangleMesh.getIndices();
	auto positions = triangleMesh.getPositions();

	unsigned geomID = rtcNewTriangleMesh2(m_scene, RTC_GEOMETRY_STATIC, indices.size(), positions.size());
	auto indexBuffer = reinterpret_cast<Vec3i*>(rtcMapBuffer(m_scene, geomID, RTC_INDEX_BUFFER));
	std::copy(indices.begin(), indices.end(), indexBuffer);
	rtcUnmapBuffer(m_scene, geomID, RTC_INDEX_BUFFER);

	struct EmbreeVertex {
		EmbreeVertex(const Vec3f& v) : x(v.x), y(v.y), z(v.z), a(0.0f) { }
		float x, y, z, a;
	};
	auto vertexBuffer = reinterpret_cast<EmbreeVertex*>(rtcMapBuffer(m_scene, geomID, RTC_VERTEX_BUFFER));
	std::copy(positions.begin(), positions.end(), vertexBuffer);
	rtcUnmapBuffer(m_scene, geomID, RTC_VERTEX_BUFFER);

	rtcSetUserData(m_scene, geomID, const_cast<TriangleMesh*>(&triangleMesh));
}

void EmbreeAccel::addSphere(const Sphere& sphere)
{
	// NOT IMPLEMENTED YET
	std::cout << "Sphere not implemented yet for Embree accelerator" << std::endl;
}

void EmbreeAccel::embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
{
	switch (code)
	{
	case RTC_NO_ERROR:
		std::cout << "RTC_NO_ERROR";
		break;
	case RTC_UNKNOWN_ERROR:
		std::cout << "RTC_UNKNOWN_ERROR";
		break;
	case RTC_INVALID_ARGUMENT:
		std::cout << "RTC_INVALID_ARGUMENT";
		break;
	case RTC_INVALID_OPERATION:
		std::cout << "RTC_INVALID_OPERATION";
		break;
	case RTC_OUT_OF_MEMORY:
		std::cout << "RTC_OUT_OF_MEMORY";
		break;
	case RTC_UNSUPPORTED_CPU:
		std::cout << "RTC_UNSUPPORTED_CPU";
		break;
	case RTC_CANCELLED:
		std::cout << "RTC_CANCELLED";
		break;
	}

	std::cout << ": " << str << std::endl;
}

}