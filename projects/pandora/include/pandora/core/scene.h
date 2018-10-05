#pragma once
#include "pandora/core/material.h"
#include "pandora/core/pandora.h"
#include "pandora/core/transform.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/area_light.h"
#include "pandora/traversal/bvh.h"
#include "pandora/eviction/fifo_cache.h"
#include "pandora/eviction/evictable.h"
#include <glm/glm.hpp>
#include <gsl/span>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

// Similar to the Primitive class in PBRTv3:
// https://github.com/mmp/pbrt-v3/blob/master/src/core/primitive.h

namespace pandora {

class SceneObjectGeometry
{
public:
    virtual ~SceneObjectGeometry() {};

    virtual Bounds worldBoundsPrimitive(unsigned primitiveID) const = 0;

    virtual unsigned numPrimitives() const = 0;
    virtual bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const = 0;
    virtual SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const = 0;

    virtual void voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform = Transform(glm::mat4(1.0f))) const = 0;

    virtual size_t sizeBytes() const = 0;
};

class SceneObjectMaterial
{
public:
    virtual ~SceneObjectMaterial() {};

    virtual void computeScatteringFunctions(
        SurfaceInteraction& si,
        ShadingMemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const = 0;
    virtual const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const = 0;
    virtual gsl::span<const AreaLight> areaLights() const = 0;
};

class InCoreSceneObject : public SceneObjectGeometry, public SceneObjectMaterial {
public:
    virtual ~InCoreSceneObject() {};

    virtual Bounds worldBounds() const = 0;

};

class OOCSceneObject {
public:
    virtual ~OOCSceneObject() {};

    virtual Bounds worldBounds() const = 0;
    virtual unsigned numPrimitives() const = 0;

    virtual std::unique_ptr<SceneObjectGeometry> getGeometryBlocking() const = 0;
    virtual std::unique_ptr<SceneObjectMaterial> getMaterialBlocking() const = 0;

};

class Scene {
public:
    Scene() = default;
    Scene(size_t geometryCacheSize);
    Scene(Scene&&) = default;
    ~Scene() = default;

    void addSceneObject(std::unique_ptr<InCoreSceneObject>&& sceneNode);
    void addSceneObject(std::unique_ptr<OOCSceneObject>&& sceneNode);
    void addInfiniteLight(const std::shared_ptr<Light>& light);

    void splitLargeOOCSceneObjects(unsigned maxPrimitivesPerSceneObject);

    std::vector<const InCoreSceneObject*> getInCoreSceneObjects() const;
    std::vector<const OOCSceneObject*> getOOCSceneObjects() const;
    gsl::span<const Light* const> getLights() const;
    gsl::span<const Light* const> getInfiniteLights() const;

    const FifoCache<TriangleMesh>* geometryCache() const;
    FifoCache<TriangleMesh>* geometryCache();

    std::vector<std::vector<const OOCSceneObject*>> groupOOCSceneObjects(unsigned uniquePrimsPerGroup) const;
    std::vector<std::vector<const InCoreSceneObject*>> groupInCoreSceneObjects(unsigned uniquePrimsPerGroup) const;

private:
    std::unique_ptr<FifoCache<TriangleMesh>> m_geometryCache;

    std::vector<std::unique_ptr<InCoreSceneObject>> m_inCoreSceneObjects;
    std::vector<std::unique_ptr<OOCSceneObject>> m_oocSceneObjects;
    std::vector<const Light*> m_lights;
    std::vector<const Light*> m_infiniteLights;

    std::vector<std::shared_ptr<Light>> m_lightOwningPointers;
};

}
