#pragma once
#include "pandora/core/scene.h"

namespace pandora
{

class InstancedSceneObject;
class GeometricSceneObject : public SceneObject {
public:
    GeometricSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material);
    GeometricSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~GeometricSceneObject() override final = default;

    Bounds worldBounds() const override final;
    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const override final;

    void lockGeometry(std::function<void(const SceneObjectGeometry&)> callback) override final;
    void lockMaterial(std::function<void(const SceneObjectMaterial&)> callback) override final;

private:
    friend class InstancedSceneObject;
private:
    std::shared_ptr<const TriangleMesh> m_mesh;
    std::shared_ptr<const Material> m_material;
    std::vector<AreaLight> m_areaLightPerPrimitive;
};

class InstancedSceneObjectOOC;
class GeometricSceneObjectOOC : public SceneObject {
public:
    GeometricSceneObjectOOC(const Bounds& worldBounds, const EvictableResourceHandle<TriangleMesh>& meshHandle, const std::shared_ptr<const Material>& material);
    //GeometricSceneObjectOOC(const Bounds& worldBounds, const EvictableResourceHandle<TriangleMesh>& meshHandle, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);
    ~GeometricSceneObjectOOC() override final = default;

    Bounds worldBounds() const override final;
    const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const override final;

    void lockGeometry(std::function<void(const SceneObjectGeometry&)> callback) override final;
    void lockMaterial(std::function<void(const SceneObjectMaterial&)> callback) override final;

private:
    friend class InstancedSceneObjectOOC;
private:
    Bounds m_worldBounds;
    EvictableResourceHandle<TriangleMesh> m_meshHandle;
    std::shared_ptr<const Material> m_material;
    std::vector<AreaLight> m_areaLightPerPrimitive;
};

class GeometricSceneObjectGeometry : public SceneObjectGeometry
{
    ~GeometricSceneObjectGeometry() override final = default;

    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;
private:
    friend class GeometricSceneObject;
    friend class GeometricSceneObjectOOC;
    GeometricSceneObjectGeometry(const std::shared_ptr<const TriangleMesh>& mesh);
private:
    std::shared_ptr<const TriangleMesh> m_mesh;
};

class GeometricSceneObjectMaterial : public SceneObjectMaterial
{
    ~GeometricSceneObjectMaterial() override final = default;

    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;
private:
    friend class GeometricSceneObject;
    friend class GeometricSceneObjectOOC;
    GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material);
private:
    std::shared_ptr<const Material> m_material;

};

}