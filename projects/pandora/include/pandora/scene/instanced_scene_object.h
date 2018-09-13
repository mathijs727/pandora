#pragma once
#include "pandora/core/scene.h"
#include "pandora/scene/geometric_scene_object.h"

namespace pandora {

class InCoreInstancedSceneObject : public InCoreSceneObject {
public:
    InCoreInstancedSceneObject(
        const glm::mat4& instanceToWorld,
        const std::shared_ptr<const InCoreGeometricSceneObject>& baseObject);
    ~InCoreInstancedSceneObject() override final = default;

    Bounds worldBounds() const override final;
    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;

    Ray transformRayToInstanceSpace(const Ray& ray) const;
    const InCoreGeometricSceneObject* getBaseObject() const { return m_baseObject.get(); }

private:
    Transform m_worldTransform;
    std::shared_ptr<const InCoreGeometricSceneObject> m_baseObject;
};

class InstancedSceneObjectGeometry : public SceneObjectGeometry {
public:
    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

private:
    InstancedSceneObjectGeometry(
        const Transform& worldTransform,
        const std::shared_ptr<const GeometricSceneObjectGeometry>& baseObjectGeometry);

private:
    const Transform m_worldTransform;
    const std::shared_ptr<const GeometricSceneObjectGeometry> m_baseObjectGeometry;
};

}
