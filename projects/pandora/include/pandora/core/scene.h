#pragma once
#include "pandora/core/material.h"
#include "pandora/core/pandora.h"
#include "pandora/core/transform.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/area_light.h"
#include "pandora/traversal/bvh.h"
#include <glm/glm.hpp>
#include <gsl/span>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

// Similar to the Primitive class in PBRTv3:
// https://github.com/mmp/pbrt-v3/blob/master/src/core/primitive.h

namespace pandora {

class SceneObject {
public:
    virtual ~SceneObject() {};

    virtual Bounds worldBounds() const = 0;
    virtual Bounds worldBoundsPrimitive(unsigned primitiveID) const = 0;

    virtual unsigned numPrimitives() const = 0;
    virtual bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const = 0;
    virtual SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const = 0;

    virtual const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const = 0;
    virtual const Material* getMaterial() const = 0;
    virtual void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const = 0;

protected:
    friend void sceneObjectToVoxelGrid(VoxelGrid& voxelGrid, const Bounds& gridBounds, const SceneObject& sceneObject);
    virtual const TriangleMesh& mesh() const = 0;
};

class GeometricSceneObject : public SceneObject {
public:
    ~GeometricSceneObject() override final = default;

    GeometricSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material);
    GeometricSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);

    Bounds worldBounds() const override final;
    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

    const AreaLight* getPrimitiveAreaLight(unsigned primitiveiD) const override final;
    const Material* getMaterial() const override final;
    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;

private:
    const TriangleMesh& mesh() const override final { return *m_mesh; }

private:
    std::shared_ptr<const TriangleMesh> m_mesh;
    std::shared_ptr<const Material> m_material;

    std::vector<AreaLight> m_areaLightPerPrimitive;
};

// Page 252 of PBRTv3
class InstancedSceneObject : public SceneObject {
public:
    ~InstancedSceneObject() override final = default;

    InstancedSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const glm::mat4& instanceToWorld, const std::shared_ptr<const Material>& material);
    InstancedSceneObject(const std::shared_ptr<const TriangleMesh>& mesh, const glm::mat4& instanceToWorld, const std::shared_ptr<const Material>& material, const Spectrum& lightEmitted);

    Bounds worldBounds() const override final;
    Bounds worldBoundsPrimitive(unsigned primitiveID) const override final;

    unsigned numPrimitives() const override final;
    bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const override final;
    SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const override final;

    const AreaLight* getPrimitiveAreaLight(unsigned primitiveiD) const override final;
    const Material* getMaterial() const override final;
    void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const override final;
private:
    // TODO: support instancing with the triangle grid voxelizer
    const TriangleMesh& mesh() const override final { return *m_mesh; }

private:
    std::shared_ptr<const TriangleMesh> m_mesh;
    std::shared_ptr<const Material> m_material;
    std::vector<AreaLight> m_areaLightPerPrimitive;

    Transform m_worldTransform;
};

class Scene {
public:
    Scene() = default;
    Scene(Scene&&) = default;
    ~Scene() = default;

    void addSceneObject(std::unique_ptr<SceneObject>&& sceneNode);
    void addInfiniteLight(const std::shared_ptr<Light>& light);

    //void splitLargeSceneObjects(unsigned maxPrimitivesPerSceneObject);

    gsl::span<const std::unique_ptr<SceneObject>> getSceneObjects() const;
    gsl::span<const Light* const> getLights() const;
    gsl::span<const Light* const> getInfiniteLights() const;

private:
    std::vector<std::unique_ptr<SceneObject>> m_sceneObjects;
    std::vector<const Light*> m_lights;
    std::vector<const Light*> m_infiniteLights;

    std::vector<std::shared_ptr<Light>> m_lightOwningPointers;
};

}
