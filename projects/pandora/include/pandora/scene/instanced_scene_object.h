#pragma once
#include "pandora/graphics_core/scene.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/flatbuffers/scene_generated.h"

namespace pandora {

class InstancedSceneObjectGeometry : public SceneObjectGeometry {
public:
    InstancedSceneObjectGeometry(
        const serialization::InstancedSceneObjectGeometry* serialized,
        const std::shared_ptr<SceneObjectGeometry>& baseObjectGeometry);// Map handle to geometry pointer
    flatbuffers::Offset<serialization::InstancedSceneObjectGeometry> serialize(
        flatbuffers::FlatBufferBuilder& builder) const;

    unsigned numPrimitives() const final;
    Bounds primitiveBounds(unsigned primitiveID) const final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const final;

    void voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const final;

    size_t sizeBytes() const final;

private:
    friend class InstancedSceneObject;
    InstancedSceneObjectGeometry(
        const Transform& worldTransform,
        const std::shared_ptr<SceneObjectGeometry>& baseObjectGeometry);

private:
    const Transform m_worldTransform;
    std::shared_ptr<SceneObjectGeometry> m_baseObjectGeometry;
};

class InstancedSceneObject : public SceneObject {
public:
    InstancedSceneObject(const glm::mat4& transformMatrix, const std::shared_ptr<const GeometricSceneObject>& baseObject);
    ~InstancedSceneObject() final = default;

    Bounds worldBounds() const final;
    unsigned numPrimitives() const final;
    
    hpx::future<std::shared_ptr<SceneObjectGeometry>> getGeometry() const final;
    InstancedSceneObjectGeometry getDummyGeometry() const;

    Ray transformRayToInstanceSpace(const Ray& ray) const;
    const GeometricSceneObject* getBaseObject() const { return m_baseObject.get(); }

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        MemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const final;
    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const final;
    gsl::span<const AreaLight> areaLights() const final;

private:
    const Transform m_transform;
    const std::shared_ptr<const GeometricSceneObject> m_baseObject;


};

}
