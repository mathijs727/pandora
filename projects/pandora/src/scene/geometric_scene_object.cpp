#include "..\..\include\pandora\scene\geometric_scene_object.h"
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
    : m_geometricProperties(mesh)
    , m_materialProperties(material, createAreaLights(lightEmitted, *mesh))
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

size_t InCoreGeometricSceneObject::size() const
{
    return m_geometricProperties.size();// + m_materialProperties.size();
}

void InCoreGeometricSceneObject::computeScatteringFunctions(
    SurfaceInteraction& si,
    ShadingMemoryArena& memoryArena,
    TransportMode mode,
    bool allowMultipleLobes) const
{
    m_materialProperties.computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
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

GeometricSceneObjectGeometry::GeometricSceneObjectGeometry(const pandora::serialization::TriangleMesh* serialized)
    : m_mesh(std::make_shared<TriangleMesh>(serialized))
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

size_t GeometricSceneObjectGeometry::size() const
{
    return m_mesh->size();
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
    std::vector<AreaLight>&& areaLights)
    : m_material(material)
    , m_areaLightPerPrimitive(std::move(areaLights))
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
    if (m_areaLightPerPrimitive.empty())
        return nullptr;
    else
        return &m_areaLightPerPrimitive[primitiveID];
}

OOCGeometricSceneObject::OOCGeometricSceneObject(
    const Bounds& worldBounds,
    const EvictableResourceHandle<TriangleMesh>& geometryHandle,
    const std::shared_ptr<const Material>& material)
    : m_geometryHandle(geometryHandle)
    , m_materialProperties(material)
{
}

Bounds OOCGeometricSceneObject::worldBounds() const
{
    return m_worldBounds;
}


std::unique_ptr<SceneObjectGeometry> OOCGeometricSceneObject::getGeometryBlocking() const
{
    return std::unique_ptr<GeometricSceneObjectGeometry>(new GeometricSceneObjectGeometry(m_geometryHandle.getBlocking()));
    //return std::make_unique<GeometricSceneObjectGeometry>(m_geometryHandle.getBlocking());
}

std::unique_ptr<SceneObjectMaterial> OOCGeometricSceneObject::getMaterialBlocking() const
{
    return std::make_unique<GeometricSceneObjectMaterial>(m_materialProperties);
}

}
