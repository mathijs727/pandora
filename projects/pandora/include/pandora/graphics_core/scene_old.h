#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/transform.h"
#include <span>
#include <memory>
#include <vector>

// Similar to the Primitive class in PBRTv3:
// https://github.com/mmp/pbrt-v3/blob/master/src/core/primitive.h

namespace pandora {

class SceneObjectGeometry {
public:
    virtual ~SceneObjectGeometry() {};

    virtual unsigned numPrimitives() const = 0;
    virtual Bounds primitiveBounds(unsigned primitiveID) const = 0;
    virtual bool intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const = 0;
    virtual SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const = 0;

    virtual void voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform = Transform(glm::mat4(1.0f))) const = 0;

    virtual size_t sizeBytes() const = 0;
};

class SceneObject {
public:
    virtual ~SceneObject() = 0;

    virtual Bounds worldBounds() const = 0;
    virtual unsigned numPrimitives() const = 0;

    virtual hpx::future<std::shared_ptr<SceneObjectGeometry>> getGeometry() const = 0;

    virtual void computeScatteringFunctions(
        SurfaceInteraction& si,
        MemoryArena& memoryArena,
        TransportMode mode,
        bool allowMultipleLobes) const = 0;
    virtual const AreaLight* getPrimitiveAreaLight(unsigned primitiveID) const = 0;
    virtual std::span<const AreaLight> areaLights() const = 0;
};

class Scene {
public:
    Scene() = default;
    Scene(Scene&&) = default;
    ~Scene() = default;

    void addSceneObject(std::unique_ptr<SceneObject>&& sceneObject);
    void addInfiniteLight(const std::shared_ptr<InfiniteLight>& light);

    std::vector<const SceneObject*> getSceneObjects() const;
    std::span<const Light* const> getLights() const;
    std::span<const InfiniteLight* const> getInfiniteLights() const;

    std::vector<std::vector<const SceneObject*>> groupSceneObjects(unsigned uniquePrimsPerGroup) const;

private:
    std::vector<std::unique_ptr<SceneObject>> m_sceneObjects;
    std::vector<const Light*> m_lights;
    std::vector<InfiniteLight*> m_infiniteLights;

    std::vector<std::shared_ptr<Light>> m_lightOwningPointers;

    Bounds m_bounds;
};
}
