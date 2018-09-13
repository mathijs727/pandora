#include "..\..\include\pandora\scene\geometric_scene_object.h"
#include "..\..\include\pandora\scene\geometric_scene_object.h"
#include "pandora/scene/geometric_scene_object.h"

namespace pandora {
GeometricSceneObject::GeometricSceneObject(
    const std::shared_ptr<const TriangleMesh>& mesh,
    const std::shared_ptr<const Material>& material)
    : m_mesh(mesh)
    , m_material(material)
    , m_areaLightPerPrimitive()
{
}

GeometricSceneObject::GeometricSceneObject(
    const std::shared_ptr<const TriangleMesh>& mesh,
    const std::shared_ptr<const Material>& material,
    const Spectrum& lightEmitted)
    : m_mesh(mesh)
    , m_material(material)
    , m_areaLightPerPrimitive()
{
    for (unsigned primitiveID = 0; primitiveID < m_mesh->numTriangles(); primitiveID++)
        m_areaLightPerPrimitive.push_back(AreaLight(lightEmitted, 1, *mesh, primitiveID));
}

Bounds GeometricSceneObject::worldBounds() const
{
    return m_mesh->getBounds();
}

void GeometricSceneObject::lockGeometry(std::function<void(const SceneObjectGeometry&)> callback)
{
    callback(GeometricSceneObjectGeometry(m_mesh));
}

void GeometricSceneObject::lockMaterial(std::function<void(const SceneObjectMaterial&)> callback)
{
    callback(GeometricSceneObjectMaterial(m_material));
}

GeometricSceneObjectOOC::GeometricSceneObjectOOC(
    const Bounds& worldBounds,
    const EvictableResourceHandle<TriangleMesh>& meshHandle,
    const std::shared_ptr<const Material>& material)
    : m_worldBounds(worldBounds)
    , m_meshHandle(meshHandle)
    , m_material(material)
    , m_areaLightPerPrimitive()
{
}

// TODO: implement by loading the mesh at construction time (to get primitive data)
/*GeometricSceneObjectOOC::GeometricSceneObjectOOC(
    const Bounds& worldBounds,
    const EvictableResourceHandle<TriangleMesh>& meshHandle,
    const std::shared_ptr<const Material>& material,
    const Spectrum& lightEmitted)
    : m_worldBounds(worldBounds)
    , m_meshHandle(meshHandle)
    , m_material(material)
    , m_areaLightPerPrimitive()
{
    for (unsigned primitiveID = 0; primitiveID < m_mesh->numTriangles(); primitiveID++)
        m_areaLightPerPrimitive.push_back(AreaLight(lightEmitted, 1, *mesh, primitiveID));
}*/

Bounds GeometricSceneObjectOOC::worldBounds() const
{
    return m_worldBounds;
}


void GeometricSceneObjectOOC::lockGeometry(std::function<void(const SceneObjectGeometry&)> callback)
{
    m_meshHandle.lock([=](std::shared_ptr<TriangleMesh> mesh) {
        callback(GeometricSceneObjectGeometry(mesh));
    });
}

void GeometricSceneObjectOOC::lockMaterial(std::function<void(const SceneObjectMaterial&)> callback)
{
    callback(GeometricSceneObjectMaterial(m_material));
}


const AreaLight* GeometricSceneObjectOOC::getPrimitiveAreaLight(unsigned primitiveID) const
{
    if (m_areaLightPerPrimitive.empty())
        return nullptr;
    else
        return &m_areaLightPerPrimitive[primitiveID];
}

GeometricSceneObjectGeometry::GeometricSceneObjectGeometry(const std::shared_ptr<const TriangleMesh>& mesh)
    : m_mesh(mesh)
{
}

Bounds GeometricSceneObjectGeometry::worldBoundsPrimitive(unsigned primitiveID) const
{
    return m_mesh->getPrimitiveBounds(primitiveID);
}

const AreaLight* GeometricSceneObject::getPrimitiveAreaLight(unsigned primitiveID) const
{
    if (m_areaLightPerPrimitive.empty())
        return nullptr;
    else
        return &m_areaLightPerPrimitive[primitiveID];
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

void GeometricSceneObjectMaterial::computeScatteringFunctions(
    SurfaceInteraction& si,
    ShadingMemoryArena& memoryArena,
    TransportMode mode,
    bool allowMultipleLobes) const
{
    m_material->computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}

}
