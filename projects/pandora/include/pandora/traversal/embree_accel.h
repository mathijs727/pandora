#pragma once
#include "embree3/rtcore.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include <gsl/gsl>
#include <memory>

namespace pandora {

class TriangleMesh;

using EmbreeInsertHandle = void*;

template <typename UserState>
class EmbreeAccel {
public:
    using ShadingCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const EmbreeInsertHandle&)>;

public:
    EmbreeAccel(gsl::span<const SceneObject> sceneObject, ShadingCallback shadingCallback);
    ~EmbreeAccel();

    void placeIntersectRequests(gsl::span<const UserState> perRayUserData, gsl::span<const Ray> rays);
    void placeIntersectRequests(gsl::span<const UserState> perRayUserData, gsl::span<const Ray> rays, const EmbreeInsertHandle& insertHandle);

private:
    void intersect(const Ray& ray, SurfaceInteraction& intersectionData) const;
    void intersectPacket(gsl::span<const Ray, 8> rays, gsl::span<SurfaceInteraction> intersectionData) const;

    void addSceneObject(const SceneObject& sceneObject);

private:
    RTCDevice m_device;
    RTCScene m_scene;

    ShadingCallback m_shadingCallback;
};

}

#include "embree_accel_impl.h"
