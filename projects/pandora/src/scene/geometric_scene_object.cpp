#include "..\..\include\pandora\scene\geometric_scene_object.h"
#include "..\..\include\pandora\scene\geometric_scene_object.h"
#include "..\..\include\pandora\scene\geometric_scene_object.h"
#include "..\..\include\pandora\scene\geometric_scene_object.h"
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
    const EvictableResourceHandle<TriangleMesh>& meshHandle,
    const std::shared_ptr<const Material>& material) :
    m_worldBounds(worldBounds),
    m_meshHandle(meshHandle),
    m_materialProperties(material)
{
}

Bounds OOCGeometricSceneObject::worldBounds() const
{
    return m_worldBounds;
}

void OOCGeometricSceneObject::lockGeometry(std::function<void(const SceneObjectGeometry&)> callback) const
{
    m_meshHandle.lock([=](std::shared_ptr<TriangleMesh> triangleMesh) {
        callback(GeometricSceneObjectGeometry(triangleMesh));
    });
}

void OOCGeometricSceneObject::lockMaterial(std::function<void(const SceneObjectMaterial&)> callback) const
{
    callback(m_materialProperties);
}

}
