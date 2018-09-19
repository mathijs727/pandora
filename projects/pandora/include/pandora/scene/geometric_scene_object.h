#pragma once
#include "pandora/core/scene.h"
#include "pandora/flatbuffers/scene_generated.h"

namespace pandora {

class GeometricSceneObjectGeometry : public SceneObjectGeometry {
public:
    GeometricSceneObjectGeometry(const pandora::serialization::GeometricSceneObjectGeometry* serialized);
    GeometricSceneObjectGeometry(const GeometricSceneObjectGeometry&) = default;
    ~GeometricSceneObjectGeometry() override final = default;

    flatbuffers::Offset<serialization::GeometricSceneObjectGeometry> serialize(flatbuffers::FlatBufferBuilder& builder) const;

    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

    size_t sizeBytes() const override final;

private:
    friend class InCoreGeometricSceneObject;
    friend class OOCGeometricSceneObject;
    GeometricSceneObjectGeometry(const std::shared_ptr<const TriangleMesh>& mesh);

private:
    std::shared_ptr<const TriangleMesh> m_mesh;
};

class GeometricSceneObjectMaterial : public SceneObjectMaterial {
public:
    ~GeometricSceneObjectMaterial() override final = default;

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;

    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const override final;
    gsl::span<const AreaLight> areaLights() const override final;
private:
    friend class InCoreGeometricSceneObject;
    friend class OOCGeometricSceneObject;
    GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material);
    //GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material, std::vector<AreaLight>&& areaLights);
    GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material, gsl::span<const AreaLight> areaLights);

private:
    std::shared_ptr<const Material> m_material;
    gsl::span<const AreaLight> m_areaLights;

};

class InCoreInstancedSceneObject;
class InCoreGeometricSceneObject : public InCoreSceneObject {
public:
    InCoreGeometricSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material);
    InCoreGeometricSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~InCoreGeometricSceneObject() = default;

    Bounds worldBounds() const override final;
    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const override final;
    gsl::span<const AreaLight> areaLights() const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;

    size_t sizeBytes() const override final;

private:
    static std::vector<AreaLight> createAreaLights(const Spectrum& lightEmitted, const TriangleMesh& mesh);

    friend class InCoreInstancedSceneObject;

private:
    // Contain instead of inherit to prevent the "dreaded diamond pattern" of inheritence
    GeometricSceneObjectGeometry m_geometricProperties;
    GeometricSceneObjectMaterial m_materialProperties;
    std::vector<AreaLight> m_areaLights;
};

class OOCInstancedSceneObject;
class OOCGeometricSceneObject : public OOCSceneObject {
public:
    OOCGeometricSceneObject(const EvictableResourceHandle<TriangleMesh>& geometryHandle, const std::shared_ptr<const Material>& material);
    OOCGeometricSceneObject(const EvictableResourceHandle<TriangleMesh>& geometryHandle, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~OOCGeometricSceneObject() override final = default;

    Bounds worldBounds() const override final;
    unsigned numPrimitives() const override final;

    std::unique_ptr<SceneObjectGeometry> getGeometryBlocking() const override final;
    std::unique_ptr<SceneObjectMaterial> getMaterialBlocking() const override final;

private:
    friend class OOCInstancedSceneObject;
private:
    Bounds m_worldBounds;
    unsigned m_numPrimitives;
    EvictableResourceHandle<TriangleMesh> m_geometryHandle;

    std::shared_ptr<const Material> m_material;

    std::vector<AreaLight> m_areaLights;
    std::shared_ptr<TriangleMesh> m_areaLightMeshOwner;
};

}
