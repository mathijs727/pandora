#pragma once
#include "pandora/core/scene.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/flatbuffers/scene_generated.h"

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
    InstancedSceneObjectGeometry(
        const serialization::InstancedSceneObjectGeometry* serialized,
        std::unique_ptr<SceneObjectGeometry> handleToGeometryFunc);// Map handle to geometry pointer
    flatbuffers::Offset<serialization::InstancedSceneObjectGeometry> serialize(
        flatbuffers::FlatBufferBuilder& builder,
        uint32_t baseGeometryHandle) const;

    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

private:
    friend class OOCInstancedSceneObject;
    InstancedSceneObjectGeometry(
        const Transform& worldTransform,
        std::unique_ptr<SceneObjectGeometry>&& baseObjectGeometry);

private:
    const Transform m_worldTransform;
    std::unique_ptr<SceneObjectGeometry> m_baseObjectGeometry;
};

class OOCInstancedSceneObject : public OOCSceneObject {
public:
    OOCInstancedSceneObject(const glm::mat4& transformMatrix, const std::shared_ptr<const OOCGeometricSceneObject>& baseObject);
    //GeometricSceneObjectOOC(const Bounds& worldBounds, const EvictableResourceHandle<TriangleMesh>& meshHandle, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~OOCInstancedSceneObject() override final = default;

    Bounds worldBounds() const override final;

    std::unique_ptr<SceneObjectGeometry> getGeometryBlocking() const override final;
    std::unique_ptr<SceneObjectMaterial> getMaterialBlocking() const override final;

    const OOCGeometricSceneObject* getBaseObject() const { return m_baseObject.get(); }

private:
    friend class InstancedSceneObjectOOC;
private:
    const Transform m_transform;
    const std::shared_ptr<const OOCGeometricSceneObject> m_baseObject;
};

}
