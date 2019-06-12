#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/flatbuffers/scene_generated.h"
#include <filesystem>

namespace pandora {

using GeometryCache = tasking::GenericResourceCache<TriangleMesh>;
struct GeometryCacheHandle {
    GeometryCache& cache;
    GeometryCache::ResourceID resourceID;

    inline hpx::future<std::shared_ptr<TriangleMesh>> get() const
    {
        return cache.lookUp(resourceID);
    }
};

class GeometricSceneObjectGeometry : public SceneObjectGeometry {
public:
    GeometricSceneObjectGeometry(const pandora::serialization::GeometricSceneObjectGeometry* serialized);
    GeometricSceneObjectGeometry(const GeometricSceneObjectGeometry&) = default;
    ~GeometricSceneObjectGeometry() final = default;

    flatbuffers::Offset<serialization::GeometricSceneObjectGeometry> serialize(flatbuffers::FlatBufferBuilder& builder) const;

    unsigned numPrimitives() const final;
    Bounds primitiveBounds(unsigned primitiveID) const final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const final;

    void voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const final;

    size_t sizeBytes() const final;

private:
    friend class GeometricSceneObject;
    GeometricSceneObjectGeometry(const std::shared_ptr<const TriangleMesh>& mesh);

private:
    std::shared_ptr<const TriangleMesh> m_mesh;
};

class InstancedSceneObject;
class GeometricSceneObject : public SceneObject {
public:
    GeometricSceneObject(const GeometryCacheHandle& geometryHandle, const std::shared_ptr<const Material>& material);
    GeometricSceneObject(const GeometryCacheHandle& geometryHandle, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~GeometricSceneObject() final = default;

    Bounds worldBounds() const final;
    unsigned numPrimitives() const final;

    hpx::future<std::shared_ptr<SceneObjectGeometry>> getGeometry() const final;

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        MemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const final;
    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const final;
    gsl::span<const AreaLight> areaLights() const final;

private:
    friend class InstancedSceneObject;

    GeometricSceneObject(
        const Bounds& bounds,
        unsigned numPrimitives,
        const GeometryCacheHandle& geometryHandle,
        const std::shared_ptr<const Material>& material);

private:
    Bounds m_worldBounds;
    unsigned m_numPrimitives = 0;
    const GeometryCacheHandle m_geometryHandle;

    std::shared_ptr<const Material> m_material;

    std::vector<AreaLight> m_areaLights;
    std::shared_ptr<TriangleMesh> m_areaLightMeshOwner;
};

}
