#include "..\..\include\pandora\scene\instanced_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"

namespace pandora {

InCoreInstancedSceneObject::InCoreInstancedSceneObject(
    const glm::mat4& instanceToWorld,
    const std::shared_ptr<const InCoreGeometricSceneObject>& baseObject)
    : m_worldTransform(Transform(instanceToWorld))
    , m_baseObject(baseObject)
{
}

Bounds InCoreInstancedSceneObject::worldBounds() const
{
    return m_worldTransform.transform(m_baseObject->worldBounds());
}

Bounds InCoreInstancedSceneObject::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_worldTransform.transform(m_baseObject->worldBoundsPrimitive(primitiveID));
}

const AreaLight* InCoreInstancedSceneObject::getPrimitiveAreaLight(unsigned primitiveID) const
{
    // TODO: does this work correctly with instancing???
    //return m_baseObject->getPrimitiveAreaLight(primitiveID);
    return nullptr;
}

unsigned InCoreInstancedSceneObject::numPrimitives() const
{
    return m_baseObject->numPrimitives();
}

bool InCoreInstancedSceneObject::intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const
{
    Ray instanceSpaceRay = m_worldTransform.transform(ray);
    bool hit = m_baseObject->intersectPrimitive(instanceSpaceRay, rayHit, primitiveID);
    ray.tfar = instanceSpaceRay.tfar;
    return hit;
}

SurfaceInteraction InCoreInstancedSceneObject::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{
    Ray instanceSpaceRay = m_worldTransform.transform(ray);
    return m_worldTransform.transform(m_baseObject->fillSurfaceInteraction(instanceSpaceRay, rayHit));
}

void InCoreInstancedSceneObject::computeScatteringFunctions(SurfaceInteraction& si, ShadingMemoryArena& memoryArena, TransportMode mode, bool allowMultipleLobes) const
{
    m_baseObject->computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}

Ray InCoreInstancedSceneObject::transformRayToInstanceSpace(const Ray& ray) const
{
    return m_worldTransform.transform(ray);
}

InstancedSceneObjectGeometry::InstancedSceneObjectGeometry(
    const Transform& worldTransform,
    const SceneObjectGeometry& baseObjectGeometry)
    : m_worldTransform(worldTransform)
    , m_baseObjectGeometry(baseObjectGeometry)
{
}

Bounds InstancedSceneObjectGeometry::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_worldTransform.transform(m_baseObjectGeometry.worldBoundsPrimitive(primitiveID));
}

unsigned InstancedSceneObjectGeometry::numPrimitives() const
{
    return m_baseObjectGeometry.numPrimitives();
}

bool InstancedSceneObjectGeometry::intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const
{
    Ray instanceSpaceRay = m_worldTransform.transform(ray);
    bool hit = m_baseObjectGeometry.intersectPrimitive(instanceSpaceRay, rayHit, primitiveID);
    ray.tfar = instanceSpaceRay.tfar;
    return hit;
}

SurfaceInteraction InstancedSceneObjectGeometry::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{
    Ray instanceSpaceRay = m_worldTransform.transform(ray);
    return m_worldTransform.transform(m_baseObjectGeometry.fillSurfaceInteraction(instanceSpaceRay, rayHit));
}

OOCInstancedSceneObject::OOCInstancedSceneObject(const glm::mat4& transformMatrix, const std::shared_ptr<const OOCGeometricSceneObject>& baseObject)
    : m_transform(transformMatrix)
    , m_baseObject(baseObject)
{
}

Bounds OOCInstancedSceneObject::worldBounds() const
{
    return m_transform.transform(m_baseObject->worldBounds());
}

void OOCInstancedSceneObject::lockGeometry(std::function<void(const SceneObjectGeometry&)> callback) const
{
    m_baseObject->lockGeometry([=](const SceneObjectGeometry& geometryProperties) {
        callback(InstancedSceneObjectGeometry(m_transform, geometryProperties));
    });
}

void OOCInstancedSceneObject::lockMaterial(std::function<void(const SceneObjectMaterial&)> callback) const
{
    m_baseObject->lockMaterial([=](const SceneObjectMaterial& materialProperties) {
        callback(materialProperties);
    });
}

}
