#pragma once
#include "embree3/rtcore.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/acceleration_structure.h"
#include <gsl/gsl>
#include <memory>

namespace pandora {

class TriangleMesh;

using EmbreeInsertHandle = void*;

template <typename UserState>
class EmbreeAccel {
public:
    using ShadingCallback = std::function<void(const Ray&, const IntersectionData&, const UserState&, const EmbreeInsertHandle&)>;

public:
    EmbreeAccel(gsl::span<const std::shared_ptr<const TriangleMesh>> meshes, ShadingCallback shadingCallback);
    ~EmbreeAccel();

    void placeIntersectRequests(gsl::span<const UserState> perRayUserData, gsl::span<const Ray> rays);
    void placeIntersectRequests(gsl::span<const UserState> perRayUserData, gsl::span<const Ray> rays, const EmbreeInsertHandle& insertHandle);

private:
    void intersect(const Ray& ray, IntersectionData& intersectionData) const;
    void intersectPacket(gsl::span<const Ray, 8> rays, gsl::span<IntersectionData> intersectionData) const;

    void addTriangleMesh(const TriangleMesh& triangleMesh);

private:
    RTCDevice m_device;
    RTCScene m_scene;

    ShadingCallback m_shadingCallback;
};

}

#include "embree_accel_impl.h"
