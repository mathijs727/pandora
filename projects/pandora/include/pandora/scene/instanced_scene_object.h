#pragma once
#include "pandora/core/scene.h"

namespace pandora {

/*class InstancedSceneObjectGeometry : public SceneObjectGeometry {
    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;
};

class InstancedSceneObjectMaterial : public SceneObjectMaterial {
    const Material* getMaterial() const override final;
    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;
};

class InstancedSceneObject : public SceneObject {
public:
    InstancedSceneObject(const std::shared_ptr<const GeometricSceneObject>& object, const glm::mat4& instanceToWorld);
    ~InstancedSceneObject() override final = default;

    Bounds worldBounds() const override final;

    const AreaLight* getPrimitiveAreaLight(unsigned primitiveiD) const override final;

    bool isInstancedSceneObject() const override final { return true; }
    const GeometricSceneObject* getBaseObject() const { return m_baseObject.get(); }
    Ray transformRayToInstanceSpace(const Ray& ray) const;

private:
    // TODO: support instancing with the triangle grid voxelizer
    const TriangleMesh& mesh() const override final { return m_baseObject->mesh(); }

private:
    std::shared_ptr<const GeometricSceneObject> m_baseObject;
    Transform m_worldTransform;
};*/

}
