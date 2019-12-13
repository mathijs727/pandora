#include "pandora/traversal/offline_bvh_cache.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include <functional>
#include <spdlog/spdlog.h>
#include <stream/serialize/in_memory_serializer.h>

namespace pandora {

OfflineBVHLeaf::OfflineBVHLeaf(const SceneObject* pSceneObject, uint32_t primID)
    : m_object(Primitive { pSceneObject, pSceneObject->pShape.get(), primID })
{
}

OfflineBVHLeaf::OfflineBVHLeaf(const CachedBVH* pBVHScene, const glm::mat4& transform)
    : m_object(Instance { pBVHScene, transform })
{
}

OfflineBVHLeaf::OfflineBVHLeaf(const CachedBVH* pBVHScene)
    : m_object(Instance { pBVHScene, {} })
{
}

Bounds OfflineBVHLeaf::getBounds() const
{
    if (std::holds_alternative<Primitive>(m_object)) {
        auto prim = std::get<Primitive>(m_object);
        return prim.pShape->getPrimitiveBounds(prim.primID);
    } else {
        const auto& instance = std::get<Instance>(m_object);
        return instance.pCachedBVH->getBounds();
    }
}

bool OfflineBVHLeaf::intersect(Ray& ray, SurfaceInteraction& si) const
{
    if (std::holds_alternative<Primitive>(m_object)) {
        const auto& prim = std::get<Primitive>(m_object);
        RayHit rayHit {};
        if (prim.pShape->intersectPrimitive(ray, rayHit, prim.primID)) {
            si = prim.pShape->fillSurfaceInteraction(ray, rayHit);
            si.pSceneObject = prim.pSceneObject;
            return true;
        } else {
            return false;
        }
    } else {
        const auto& instance = std::get<Instance>(m_object);
        if (instance.pCachedBVH->intersect(ray, si)) {
            if (instance.transform.has_value()) {
                Transform transform { instance.transform.value() };
                si = transform.transform(si);
            }
            return true;
        } else {
            return false;
        }
    }
}

bool OfflineBVHLeaf::intersectAny(Ray& ray) const
{
    if (std::holds_alternative<Primitive>(m_object)) {
        auto prim = std::get<Primitive>(m_object);
        RayHit dummyRayHit;
        return prim.pShape->intersectPrimitive(ray, dummyRayHit, prim.primID);
    } else {
        const auto& instance = std::get<Instance>(m_object);
        return instance.pCachedBVH->intersectAny(ray);
    }
}

CachedBVH::CachedBVH(WiVeBVH8Build8<OfflineBVHLeaf>&& bvh, const Bounds& bounds)
    : Evictable(true)
    , m_bvh(std::move(bvh))
    , m_bounds(bounds)
{
}

Bounds CachedBVH::getBounds() const
{
    return m_bounds;
}

bool CachedBVH::intersect(Ray& ray, SurfaceInteraction& si) const
{
    ALWAYS_ASSERT(m_bvh.has_value());
    return m_bvh->intersect(ray, si);
}

bool CachedBVH::intersectAny(Ray& ray) const
{
    ALWAYS_ASSERT(m_bvh.has_value());
    return m_bvh->intersectAny(ray);
}

size_t CachedBVH::sizeBytes() const
{
    if (m_bvh.has_value()) {
        return sizeof(*this) + m_bvh->sizeBytes();
    } else {
        return sizeof(*this);
    }
}

void CachedBVH::serialize(tasking::Serializer& serializer)
{
    //TODO(Mathijs):
    flatbuffers::FlatBufferBuilder fbb;
    auto serializedBVH = m_bvh->serialize(fbb);
    fbb.Finish(serializedBVH);

    auto [bvhAllocationHandle, pBVHMem] = serializer.allocateAndMap(fbb.GetSize());
    std::memcpy(pBVHMem, fbb.GetBufferPointer(), fbb.GetSize());
    m_bvhSerializeAllocation = bvhAllocationHandle;

    gsl::span<const OfflineBVHLeaf> leafs = m_bvh->leafs();
    auto [leafsAllocationHandle, pLeafsMem] = serializer.allocateAndMap(leafs.size_bytes());
    std::memcpy(pLeafsMem, leafs.data(), leafs.size_bytes());
    m_numLeafs = leafs.size();
    m_leafsSerializeAllocation = leafsAllocationHandle;
}

void CachedBVH::doEvict()
{
    m_bvh.reset();
}

void CachedBVH::doMakeResident(tasking::Deserializer& deserializer)
{
    //TODO(Mathijs): ...
    const void* pLeafsMem = deserializer.map(m_leafsSerializeAllocation);
    const OfflineBVHLeaf* pLeafs = static_cast<const OfflineBVHLeaf*>(pLeafsMem);

    const void* pBVHMem = deserializer.map(m_bvhSerializeAllocation);
    WiVeBVH8Build8<OfflineBVHLeaf> bvh { pandora::serialization::GetWiVeBVH8(pBVHMem), gsl::span(pLeafs, m_numLeafs) };
    m_bvh.emplace(std::move(bvh));
}

bool CachedBVHSubScene::intersect(Ray& ray, SurfaceInteraction& si) const
{
    return m_pBVH->intersect(ray, si);
}

bool CachedBVHSubScene::intersectAny(Ray& ray) const
{
    return m_pBVH->intersectAny(ray);
}

CachedBVHSubScene::CachedBVHSubScene(tasking::CachedPtr<CachedBVH>&& pBVH, std::vector<tasking::CachedPtr<CachedBVH>>&& childBVHs)
    : m_pBVH(std::move(pBVH))
    , m_childBVHs(std::move(childBVHs))
{
}

pandora::LRUBVHSceneCache::LRUBVHSceneCache(gsl::span<const SubScene*> subScenes, tasking::LRUCache* pSceneCache, size_t maxSize)
{
    spdlog::warn("Using in-memory serializer for BVH cache");
    auto pSerializer = std::make_unique<tasking::InMemorySerializer>();

    using CacheBuilder = tasking::LRUCache::Builder;
    CacheBuilder cacheBuilder = CacheBuilder(std::move(pSerializer));

    for (const SubScene* pSubScene : subScenes) {
        createBVH(pSubScene, pSceneCache, &cacheBuilder);
    }

    m_lruCache = cacheBuilder.build(maxSize);
}

CachedBVHSubScene LRUBVHSceneCache::fromSubScene(const SubScene* pSubScene)
{
    // Load main BVH
    auto pCachedBVH = m_lruCache->makeResident(m_bvhSceneLUT.find(static_cast<const void*>(pSubScene))->second.get());

    // Load child BVHs (BVHs referred to by the leafs of the main BVH)
    const auto& childBVHs = m_childBVHs.find(pSubScene)->second;
    std::vector<tasking::CachedPtr<CachedBVH>> cachedChildBVHs;
    std::transform(std::begin(childBVHs), std::end(childBVHs), std::back_inserter(cachedChildBVHs),
        [this](CachedBVH* pCachedBVH) {
            return m_lruCache->makeResident<CachedBVH>(pCachedBVH);
        });

    return CachedBVHSubScene(std::move(pCachedBVH), std::move(cachedChildBVHs));
}

CachedBVH* LRUBVHSceneCache::createBVH(const SceneNode* pSceneNode, tasking::LRUCache* pGeomCache, tasking::CacheBuilder* pCacheBuilder)
{
    std::vector<OfflineBVHLeaf> leafs;
    Bounds bounds;

    for (const auto& [pChild, optTransform] : pSceneNode->children) {
        CachedBVH* pChildBVH = nullptr;
        if (auto iter = m_bvhSceneLUT.find(static_cast<const void*>(pChild.get())); iter != std::end(m_bvhSceneLUT)) {
            pChildBVH = iter->second.get();
        } else {
            pChildBVH = createBVH(pChild.get(), pGeomCache, pCacheBuilder);
        }

        auto childBounds = pChild->computeBounds();
        if (optTransform) {
            Transform transform { *optTransform };
            transform.transform(childBounds);
        }
        bounds.extend(childBounds);

        if (optTransform)
            leafs.emplace_back(pChildBVH, *optTransform);
        else
            leafs.emplace_back(pChildBVH);
    }

    std::vector<tasking::CachedPtr<Shape>> residentShapes;
    for (const auto& pSceneObject : pSceneNode->objects) {
        const auto pShapeOwner = pGeomCache->makeResident(pSceneObject->pShape.get());
        bounds.extend(pShapeOwner->getBounds());
        for (unsigned i = 0; i < pShapeOwner->numPrimitives(); i++)
            leafs.emplace_back(pSceneObject.get(), i);
        residentShapes.emplace_back(std::move(pShapeOwner));
    }

    // NOTE: takes ownership of leafs (moves leafs into it's own array)
    WiVeBVH8Build8<OfflineBVHLeaf> bvh { leafs };
    auto pCachedBVHOwner = std::make_unique<CachedBVH>(std::move(bvh), bounds);
    auto pCachedBVH = pCachedBVHOwner.get();
    m_bvhSceneLUT[static_cast<const void*>(pSceneNode)] = std::move(pCachedBVHOwner);
    return pCachedBVH;
}

CachedBVH* LRUBVHSceneCache::createBVH(const SubScene* pSubScene, tasking::LRUCache* pGeomCache, tasking::CacheBuilder* pCacheBuilder)
{
    std::vector<OfflineBVHLeaf> leafs;
    Bounds bounds;

    for (const auto& [pChild, optTransform] : pSubScene->sceneNodes) {
        CachedBVH* pChildBVH = nullptr;
        if (auto iter = m_bvhSceneLUT.find(static_cast<const void*>(pChild)); iter != std::end(m_bvhSceneLUT)) {
            pChildBVH = iter->second.get();
        } else {
            pChildBVH = createBVH(pChild, pGeomCache, pCacheBuilder);
        }

        auto childBounds = pChild->computeBounds();
        if (optTransform) {
            Transform transform { *optTransform };
            transform.transform(childBounds);
        }
        bounds.extend(childBounds);

        if (optTransform)
            leafs.emplace_back(pChildBVH, *optTransform);
        else
            leafs.emplace_back(pChildBVH);
    }

    std::vector<tasking::CachedPtr<Shape>> residentShapes;
    for (const auto* pSceneObject : pSubScene->sceneObjects) {
        const auto pShapeOwner = pGeomCache->makeResident(pSceneObject->pShape.get());
        bounds.extend(pShapeOwner->getBounds());
        for (unsigned i = 0; i < pShapeOwner->numPrimitives(); i++)
            leafs.emplace_back(pSceneObject, i);
        residentShapes.emplace_back(std::move(pShapeOwner));
    }

    // NOTE: takes ownership of leafs (moves leafs into it's own array)
    WiVeBVH8Build8<OfflineBVHLeaf> bvh { leafs };
    auto pCachedBVHOwner = std::make_unique<CachedBVH>(std::move(bvh), bounds);
    auto pCachedBVH = pCachedBVHOwner.get();
    m_bvhSceneLUT[static_cast<const void*>(pSubScene)] = std::move(pCachedBVHOwner);
    return pCachedBVH;
}

}