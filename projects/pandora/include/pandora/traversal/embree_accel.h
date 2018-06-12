#pragma once
#include "embree3/rtcore.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/acceleration_structure.h"
#include <gsl/gsl>
#include <memory>

namespace pandora {

class Scene;
class TriangleMesh;
struct Sphere;

class EmbreeAccel {
public:
    EmbreeAccel(gsl::span<const std::shared_ptr<const TriangleMesh>> meshes);
    ~EmbreeAccel();

    void intersect(Ray& ray, IntersectionData& intersectionData) const;
    void intersectPacket(gsl::span<Ray, 8> rays, gsl::span<IntersectionData> intersectionData) const;

private:
    void addTriangleMesh(const TriangleMesh& triangleMesh);
    void addSphere(const Sphere& sphere);

    static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);

private:
    RTCDevice m_device;
    RTCScene m_scene;
};
}
