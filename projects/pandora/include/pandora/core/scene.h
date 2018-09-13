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

struct SceneObjectGeometry
{
public:
    virtual ~SceneObjectGeometry() {};

    virtual Bounds worldBoundsPrimitive(unsigned primitiveID) const = 0;

    virtual unsigned numPrimitives() const = 0;
    virtual bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const = 0;
    virtual SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const = 0;
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
};

class SceneObject {
public:
    virtual ~SceneObject() {};

    virtual Bounds worldBounds() const = 0;
    virtual const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const = 0;
    virtual bool isInstancedSceneObject() const { return false; }

    virtual void lockGeometry(std::function<void(const SceneObjectGeometry&)> callback) = 0;
    virtual void lockMaterial(std::function<void(const SceneObjectMaterial&)> callback) = 0;

//protected:
//    friend void sceneObjectToVoxelGrid(VoxelGrid& voxelGrid, const Bounds& gridBounds, const SceneObject& sceneObject);
//    virtual const TriangleMesh& mesh() const = 0;
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
