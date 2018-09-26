#include "pandora/scene/geometric_scene_object.h"

namespace pandora {

InCoreGeometricSceneObject::InCoreGeometricSceneObject(
    const std::shared_ptr<const TriangleMesh>& mesh,
    const std::shared_ptr<const Material>& material)
    : m_geometricProperties(mesh)
    , m_materialProperties(material)
{
}

InCoreGeometricSceneObject::InCoreGeometricSceneObject(
    const std::shared_ptr<const TriangleMesh>& mesh,
    const std::shared_ptr<const Material>& material,
    const Spectrum& lightEmitted)
    : m_areaLights(createAreaLights(lightEmitted, *mesh))
    , m_geometricProperties(mesh)
    , m_materialProperties(material, m_areaLights)
{
}

Bounds InCoreGeometricSceneObject::worldBounds() const
{
    return m_geometricProperties.m_mesh->getBounds();
}

Bounds InCoreGeometricSceneObject::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_geometricProperties.worldBoundsPrimitive(primitiveID);
}

const AreaLight* InCoreGeometricSceneObject::getPrimitiveAreaLight(unsigned primitiveID) const
{
    return m_materialProperties.getPrimitiveAreaLight(primitiveID);
}

gsl::span<const AreaLight> InCoreGeometricSceneObject::areaLights() const
{
    return m_areaLights;
}

unsigned InCoreGeometricSceneObject::numPrimitives() const
{
    return m_geometricProperties.numPrimitives();
}

bool InCoreGeometricSceneObject::intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const
{
    return m_geometricProperties.intersectPrimitive(ray, rayHit, primitiveID);
}

SurfaceInteraction InCoreGeometricSceneObject::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{
    return m_geometricProperties.fillSurfaceInteraction(ray, rayHit);
}

size_t InCoreGeometricSceneObject::sizeBytes() const
{
    return sizeof(decltype(*this)) + m_geometricProperties.sizeBytes(); // + m_materialProperties.size();
}

void InCoreGeometricSceneObject::computeScatteringFunctions(
    SurfaceInteraction& si,
    ShadingMemoryArena& memoryArena,
    TransportMode mode,
    bool allowMultipleLobes) const
{
    m_materialProperties.computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}

void InCoreGeometricSceneObject::voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const
{
    return m_geometricProperties.voxelize(grid, gridBounds, transform);
}

std::vector<AreaLight> InCoreGeometricSceneObject::createAreaLights(const Spectrum& lightEmitted, const TriangleMesh& mesh)
{
    std::vector<AreaLight> lights;
    for (unsigned primitiveID = 0; primitiveID < mesh.numTriangles(); primitiveID++)
        lights.push_back(AreaLight(lightEmitted, 1, mesh, primitiveID));
    return lights;
}

GeometricSceneObjectGeometry::GeometricSceneObjectGeometry(const std::shared_ptr<const TriangleMesh>& mesh)
    : m_mesh(mesh)
{
}

GeometricSceneObjectGeometry::GeometricSceneObjectGeometry(const serialization::GeometricSceneObjectGeometry* serialized)
    : m_mesh(std::make_shared<TriangleMesh>(serialized->geometry()))
{
}

Bounds GeometricSceneObjectGeometry::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_mesh->getPrimitiveBounds(primitiveID);
}

unsigned GeometricSceneObjectGeometry::numPrimitives() const
{
    return m_mesh->numTriangles();
}

bool GeometricSceneObjectGeometry::intersectPrimitive(Ray& ray, RayHit& rayHit, unsigned primitiveID) const
{
    return m_mesh->intersectPrimitive(ray, rayHit, primitiveID);
}

SurfaceInteraction GeometricSceneObjectGeometry::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{
    return m_mesh->fillSurfaceInteraction(ray, rayHit);
}

void GeometricSceneObjectGeometry::voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const
{
    return m_mesh->voxelize(grid, gridBounds, transform);
}

size_t GeometricSceneObjectGeometry::sizeBytes() const
{
    return sizeof(decltype(*this)) + m_mesh->sizeBytes();
}

flatbuffers::Offset<serialization::GeometricSceneObjectGeometry> GeometricSceneObjectGeometry::serialize(flatbuffers::FlatBufferBuilder& builder) const
{
    return serialization::CreateGeometricSceneObjectGeometry(builder, m_mesh->serialize(builder));
}

GeometricSceneObjectMaterial::GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material)
    : m_material(material)
{
}

GeometricSceneObjectMaterial::GeometricSceneObjectMaterial(
    const std::shared_ptr<const Material>& material,
    gsl::span<const AreaLight> areaLights)
    : m_material(material)
    , m_areaLights(areaLights)
{
}

void GeometricSceneObjectMaterial::computeScatteringFunctions(
    SurfaceInteraction& si,
    ShadingMemoryArena& memoryArena,
    TransportMode mode,
    bool allowMultipleLobes) const
{
    m_material->computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}

const AreaLight* GeometricSceneObjectMaterial::getPrimitiveAreaLight(unsigned primitiveID) const
{
    if (m_areaLights.empty())
        return nullptr;
    else
        return &m_areaLights[primitiveID];
}

gsl::span<const AreaLight> GeometricSceneObjectMaterial::areaLights() const
{
    return m_areaLights;
}

OOCGeometricSceneObject::OOCGeometricSceneObject(
    const EvictableResourceHandle<TriangleMesh>& geometryHandle,
    const std::shared_ptr<const Material>& material)
    : m_geometryHandle(geometryHandle)
    , m_material(material)
{
    auto geometry = getGeometryBlocking();
    for (unsigned primID = 0; primID < geometry->numPrimitives(); primID++) {
        m_worldBounds.extend(geometry->worldBoundsPrimitive(primID));
    }
    m_numPrimitives = geometry->numPrimitives();
}

OOCGeometricSceneObject::OOCGeometricSceneObject(
    const EvictableResourceHandle<TriangleMesh>& geometryHandle,
    const std::shared_ptr<const Material>& material,
    const Spectrum& lightEmitted)
    : m_geometryHandle(geometryHandle)
    , m_material(material)
{
    auto geometry = getGeometryBlocking();
    for (unsigned primID = 0; primID < geometry->numPrimitives(); primID++) {
        m_worldBounds.extend(geometry->worldBoundsPrimitive(primID));
    }
    m_numPrimitives = geometry->numPrimitives();

    // Get the mesh (should already be in cache because of the above call)
    m_areaLightMeshOwner = geometryHandle.getBlocking();
    for (unsigned primitiveID = 0; primitiveID < m_areaLightMeshOwner->numTriangles(); primitiveID++)
        m_areaLights.push_back(AreaLight(lightEmitted, 1, *m_areaLightMeshOwner, primitiveID));
}

Bounds OOCGeometricSceneObject::worldBounds() const
{
    return m_worldBounds;
}

unsigned OOCGeometricSceneObject::numPrimitives() const
{
    return m_numPrimitives;
}

std::unique_ptr<SceneObjectGeometry> OOCGeometricSceneObject::getGeometryBlocking() const
{
    // Can't use std::make_unique because GeometricSceneObjectGeometry constructor is private (OOCGeometricSceneObject is a friend)
    return std::unique_ptr<GeometricSceneObjectGeometry>(new GeometricSceneObjectGeometry(m_geometryHandle.getBlocking()));
}

std::unique_ptr<SceneObjectMaterial> OOCGeometricSceneObject::getMaterialBlocking() const
{
    if (m_areaLightMeshOwner) {
        return std::unique_ptr<GeometricSceneObjectMaterial>(
            new GeometricSceneObjectMaterial(
                m_material,
                m_areaLights));
    } else {
        return std::unique_ptr<GeometricSceneObjectMaterial>(
            new GeometricSceneObjectMaterial(
                m_material));
    }
}

}
