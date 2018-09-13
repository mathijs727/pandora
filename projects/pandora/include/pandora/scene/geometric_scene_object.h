#pragma once
#include "pandora/core/scene.h"

namespace pandora {

class GeometricSceneObjectGeometry : public SceneObjectGeometry {
public:
    ~GeometricSceneObjectGeometry() override final = default;

    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

private:
    friend class InCoreGeometricSceneObject;
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
private:
    friend class InCoreGeometricSceneObject;
    GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material);
    GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material, std::vector<AreaLight>&& areaLights);

private:
    std::shared_ptr<const Material> m_material;
    std::vector<AreaLight> m_areaLightPerPrimitive;
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

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;

private:
    static std::vector<AreaLight> createAreaLights(const Spectrum& lightEmitted, const TriangleMesh& mesh);

    friend class InCoreInstancedSceneObject;

private:
    // Contain instead of inherit to prevent the "dreaded diamond pattern" of inheritence
    GeometricSceneObjectGeometry m_geometricProperties;
    GeometricSceneObjectMaterial m_materialProperties;
};

/*class InstancedSceneObjectOOC;
class GeometricSceneObjectOOC : public OOCSceneObject {
public:
    GeometricSceneObjectOOC(const Bounds& worldBounds, const EvictableResourceHandle<TriangleMesh>& meshHandle, const std::shared_ptr<const Material>& material);
    //GeometricSceneObjectOOC(const Bounds& worldBounds, const EvictableResourceHandle<TriangleMesh>& meshHandle, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~GeometricSceneObjectOOC() override final = default;

    Bounds worldBounds() const override final;
    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const override final;

    void lockGeometry(std::function<void(const SceneObjectGeometry&)> callback) const override final;
    void lockMaterial(std::function<void(const SceneObjectMaterial&)> callback) const override final;

private:
    friend class InstancedSceneObjectOOC;
private:
    Bounds m_worldBounds;
    EvictableResourceHandle<TriangleMesh> m_meshHandle;
    std::shared_ptr<const Material> m_material;
    std::vector<AreaLight> m_areaLightPerPrimitive;
};*/

}
