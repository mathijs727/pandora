#pragma once
#include "pandora/traversal/acceleration_structure.h"
#include "embree2/rtcore.h"

namespace pandora {

class Scene;
class TriangleMesh;
struct Sphere;

class EmbreeAccel : public AccelerationStructure
{
public:
	EmbreeAccel(const Scene& scene);
	~EmbreeAccel();

	void intersect(Ray& ray, IntersectionData& intersectionData) final;
	void intersect(gsl::span<Ray> rays, gsl::span<IntersectionData> intersectionData) final;
private:
	void addTriangleMesh(const TriangleMesh& triangleMesh);
	void addSphere(const Sphere& sphere);

	static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);
private:
	RTCDevice m_device;
	RTCScene m_scene;
};
}