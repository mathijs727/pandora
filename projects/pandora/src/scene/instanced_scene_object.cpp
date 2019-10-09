#include "pandora/scene/instanced_scene_object.h"
#include "pandora/flatbuffers/data_conversion.h"

namespace pandora {

InstancedSceneObjectGeometry::InstancedSceneObjectGeometry(
    const Transform& worldTransform,
    const std::shared_ptr<SceneObjectGeometry>& baseObjectGeometry)
    : m_worldTransform(worldTransform)
    , m_baseObjectGeometry(std::move(baseObjectGeometry))
{
}

InstancedSceneObjectGeometry::InstancedSceneObjectGeometry(
    const serialization::InstancedSceneObjectGeometry* serialized,
    const std::shared_ptr<SceneObjectGeometry>& baseObjectGeometry)
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

unsigned InstancedSceneObjectGeometry::numPrimitives() const
{
    return m_baseObjectGeometry->numPrimitives();
}

Bounds InstancedSceneObjectGeometry::primitiveBounds(unsigned primitiveID) const
{
    return m_worldTransform.transform(m_baseObjectGeometry->primitiveBounds(primitiveID));
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

void InstancedSceneObjectGeometry::voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const
{
    return m_baseObjectGeometry->voxelize(grid, gridBounds, m_worldTransform * transform);
}

size_t InstancedSceneObjectGeometry::sizeBytes() const
{
    return sizeof(decltype(*this)) + m_baseObjectGeometry->sizeBytes();
}

InstancedSceneObject::InstancedSceneObject(const glm::mat4& transformMatrix, const std::shared_ptr<const GeometricSceneObject>& baseObject)
    : m_transform(transformMatrix)
    , m_baseObject(baseObject)
{
}

Bounds InstancedSceneObject::worldBounds() const
{
    return m_transform.transform(m_baseObject->worldBounds());
}

hpx::future<std::shared_ptr<SceneObjectGeometry>> InstancedSceneObject::getGeometry() const
{
    // Cannot use std::make_shared on private constructor of friended class
    auto baseGeometry = co_await m_baseObject->getGeometry();
    co_return std::shared_ptr<InstancedSceneObjectGeometry>(
        new InstancedSceneObjectGeometry(
            m_transform, baseGeometry));
}

InstancedSceneObjectGeometry InstancedSceneObject::getDummyGeometry() const
{
    // Cannot use std::make_unique on private constructor of friended class
    std::shared_ptr<SceneObjectGeometry> baseGeometryDummy = nullptr;
    return InstancedSceneObjectGeometry(m_transform, std::move(baseGeometryDummy));
}

Ray InstancedSceneObject::transformRayToInstanceSpace(const Ray& ray) const
{
    return m_transform.transform(ray);
}

unsigned pandora::InstancedSceneObject::numPrimitives() const
{
    return m_baseObject->numPrimitives();
}

void InstancedSceneObject::computeScatteringFunctions(
    SurfaceInteraction& si, std::pmr::memory_resource* pAllocator, TransportMode mode, bool allowMultipleLobes) const
{
    m_baseObject->computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}

const AreaLight* InstancedSceneObject::getPrimitiveAreaLight(unsigned primitiveID) const
{
    // TODO: does this work correctly with instancing???
    //return m_baseObject->getPrimitiveAreaLight(primitiveID);
    return nullptr;
}

gsl::span<const AreaLight> InstancedSceneObject::areaLights() const
{
    return {};
}

}
