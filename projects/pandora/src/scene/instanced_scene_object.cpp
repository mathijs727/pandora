#include "pandora/scene/instanced_scene_object.h"
#include "pandora/flatbuffers/data_conversion.h"

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

gsl::span<const AreaLight> InCoreInstancedSceneObject::areaLights() const
{
    return {};
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

size_t InCoreInstancedSceneObject::sizeBytes() const
{
    return sizeof(decltype(*this)) + m_baseObject->sizeBytes();
}

InstancedSceneObjectGeometry::InstancedSceneObjectGeometry(
    const Transform& worldTransform,
    std::unique_ptr<SceneObjectGeometry>&& baseObjectGeometry)
    : m_worldTransform(worldTransform)
    , m_baseObjectGeometry(std::move(baseObjectGeometry))
{
}

InstancedSceneObjectGeometry::InstancedSceneObjectGeometry(
    const serialization::InstancedSceneObjectGeometry* serialized,
    std::unique_ptr<SceneObjectGeometry>&& baseObjectGeometry)
    : m_worldTransform(Transform(serialized->transform()))
    , m_baseObjectGeometry(std::move(baseObjectGeometry))
{
}

flatbuffers::Offset<serialization::InstancedSceneObjectGeometry> InstancedSceneObjectGeometry::serialize(
    flatbuffers::FlatBufferBuilder& builder) const
{
    // TODO: support multiple levels of instancing
    auto serializedTransform = m_worldTransform.serialize();
    return serialization::CreateInstancedSceneObjectGeometry(
        builder,
        &serializedTransform);
}

Bounds InstancedSceneObjectGeometry::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_worldTransform.transform(m_baseObjectGeometry->worldBoundsPrimitive(primitiveID));
}

unsigned InstancedSceneObjectGeometry::numPrimitives() const
{
    return m_baseObjectGeometry->numPrimitives();
}

bool InstancedSceneObjectGeometry::intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const
{
    Ray instanceSpaceRay = m_worldTransform.transform(ray);
    bool hit = m_baseObjectGeometry->intersectPrimitive(instanceSpaceRay, rayHit, primitiveID);
    ray.tfar = instanceSpaceRay.tfar;
    return hit;
}

SurfaceInteraction InstancedSceneObjectGeometry::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{
    Ray instanceSpaceRay = m_worldTransform.transform(ray);
    return m_worldTransform.transform(m_baseObjectGeometry->fillSurfaceInteraction(instanceSpaceRay, rayHit));
}

size_t InstancedSceneObjectGeometry::sizeBytes() const
{
    return sizeof(decltype(*this)) + m_baseObjectGeometry->sizeBytes();
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

std::unique_ptr<SceneObjectGeometry> OOCInstancedSceneObject::getGeometryBlocking() const
{
    // Cannot use std::make_unique on private constructor of friended class
    return std::unique_ptr<InstancedSceneObjectGeometry>(
        new InstancedSceneObjectGeometry(
            m_transform, m_baseObject->getGeometryBlocking()));
}

std::unique_ptr<SceneObjectMaterial> OOCInstancedSceneObject::getMaterialBlocking() const
{
    return m_baseObject->getMaterialBlocking();
}

Ray OOCInstancedSceneObject::transformRayToInstanceSpace(const Ray& ray) const
{
    return m_transform.transform(ray);
}

unsigned pandora::OOCInstancedSceneObject::numPrimitives() const
{
    return m_baseObject->numPrimitives();
}


}
