#include "pandora/scene/geometric_scene_object.h"
#include "pandora/utility/error_handling.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/area_light.h"
#include <fstream>
#include <mio/mmap.hpp>

namespace pandora {

GeometricSceneObjectGeometry::GeometricSceneObjectGeometry(const std::shared_ptr<const TriangleMesh>& mesh)
    : m_mesh(mesh)
{
}

GeometricSceneObjectGeometry::GeometricSceneObjectGeometry(const serialization::GeometricSceneObjectGeometry* serialized)
    : m_mesh(std::make_shared<TriangleMesh>(serialized->geometry()))
{
}

Bounds GeometricSceneObjectGeometry::primitiveBounds(unsigned primitiveID) const
{
    ALWAYS_ASSERT(m_mesh != nullptr);
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

/*GeometricSceneObjectMaterial::GeometricSceneObjectMaterial(const std::shared_ptr<const Material>& material)
    : m_material(material)
{
}

GeometricSceneObjectMaterial::GeometricSceneObjectMaterial(
    const std::shared_ptr<const Material>& material,
    gsl::span<const AreaLight> areaLights)
    : m_material(material)
    , m_areaLights(areaLights)
{
}*/

void GeometricSceneObject::computeScatteringFunctions(
    SurfaceInteraction& si,
    MemoryArena& memoryArena,
    TransportMode mode,
    bool allowMultipleLobes) const
{
    m_material->computeScatteringFunctions(si, memoryArena, mode, allowMultipleLobes);
}

const AreaLight* GeometricSceneObject::getPrimitiveAreaLight(unsigned primitiveID) const
{
    if (m_areaLights.empty())
        return nullptr;
    else
        return &m_areaLights[primitiveID];
}

gsl::span<const AreaLight> GeometricSceneObject::areaLights() const
{
    return m_areaLights;
}

GeometricSceneObject::GeometricSceneObject(
    const GeometryCacheHandle& geometryHandle,
    const std::shared_ptr<const Material>& material)
    : m_geometryHandle(geometryHandle)
    , m_material(material)
{
    // TODO: collect geometry bounds without having to load a full mesh.
    auto geometry = geometryHandle.get().get();
    m_worldBounds = geometry->getBounds();
    m_numPrimitives = geometry->numTriangles();
}

GeometricSceneObject::GeometricSceneObject(
    const GeometryCacheHandle& geometryHandle,
    const std::shared_ptr<const Material>& material,
    const Spectrum& lightEmitted)
    : m_geometryHandle(geometryHandle)
    , m_material(material)
{
    auto geometry = geometryHandle.get().get();
    m_worldBounds = geometry->getBounds();
    m_numPrimitives = geometry->numTriangles();

    // Get the mesh (should already be in cache because of the above call)
    m_areaLightMeshOwner = geometry;
    for (unsigned primitiveID = 0; primitiveID < m_areaLightMeshOwner->numTriangles(); primitiveID++)
        m_areaLights.push_back(AreaLight(lightEmitted, 1, *m_areaLightMeshOwner, primitiveID));
}

Bounds GeometricSceneObject::worldBounds() const
{
    return m_worldBounds;
}

unsigned GeometricSceneObject::numPrimitives() const
{
    return m_numPrimitives;
}

hpx::future<std::shared_ptr<SceneObjectGeometry>> GeometricSceneObject::getGeometry() const
{
    // Can't use std::make_shared because GeometricSceneObjectGeometry constructor is private (OOCGeometricSceneObject is a friend)
    auto geometry = co_await m_geometryHandle.get();
    co_return std::shared_ptr<GeometricSceneObjectGeometry>(
        new GeometricSceneObjectGeometry(geometry));
}

/*GeometricSceneObject GeometricSceneObject::geometricSplit(CacheT<TriangleMesh>* cache, std::filesystem::path filePath, gsl::span<unsigned> primitiveIDs)
{
    // Splitting of area light meshes is not supported yet (requires some work / refactoring)
    ALWAYS_ASSERT(m_areaLights.empty() && !m_areaLightMeshOwner);

    auto geometry = m_geometryHandle.getBlocking();
    auto subGeometry = geometry->subMesh(primitiveIDs);

    auto subBounds = subGeometry.getBounds();
    auto subNumPrimitives = subGeometry.numTriangles();

    flatbuffers::FlatBufferBuilder fbb;
    auto serializedSubMesh = subGeometry.serialize(fbb);
    fbb.Finish(serializedSubMesh);

    std::ofstream file;
    file.open(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    file.close();

    std::cout << filePath << ": " << subGeometry.numTriangles() << " - " << primitiveIDs.size() << std::endl;

    auto resourceID = cache->emplaceFactoryUnsafe<TriangleMesh>([filePath, numPrims = primitiveIDs.size()]() {
        auto mmapFile = mio::mmap_source(filePath.string(), 0, mio::map_entire_file);
        auto serializedMesh = serialization::GetTriangleMesh(mmapFile.data());
        return TriangleMesh(serializedMesh);
    });
    EvictableResourceHandle<TriangleMesh, CacheT<TriangleMesh>> subGeometryHandle(cache, resourceID);
    return OOCGeometricSceneObject(subBounds, subNumPrimitives, subGeometryHandle, m_material);
}*/

GeometricSceneObject::GeometricSceneObject(
    const Bounds& bounds,
    unsigned numPrimitives,
    const GeometryCacheHandle& geometryHandle,
    const std::shared_ptr<const Material>& material)
    : m_worldBounds(bounds)
    , m_numPrimitives(numPrimitives)
    , m_geometryHandle(geometryHandle)
    , m_material(material)
{
}

}
