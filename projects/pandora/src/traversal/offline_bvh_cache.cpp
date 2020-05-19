#include "pandora/traversal/offline_bvh_cache.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/utility/error_handling.h"
#include <functional>
#include <spdlog/spdlog.h>
#include <stream/cache/lru_cache_ts.h>
#include <stream/serialize/file_serializer.h>
#include <stream/serialize/in_memory_serializer.h>
#include <unordered_set>
#include <vector>

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
                const Transform transform { instance.transform.value() };
                si = transform.transformToWorld(si);
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
    g_stats.memory.botLevelLoaded += m_bvh->sizeBytes();
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
    g_stats.memory.botLevelEvicted += m_bvh->sizeBytes();
    m_bvh.reset();
}

void CachedBVH::doMakeResident(tasking::Deserializer& deserializer)
{
    const void* pLeafsMem = deserializer.map(m_leafsSerializeAllocation);
    const OfflineBVHLeaf* pLeafs = static_cast<const OfflineBVHLeaf*>(pLeafsMem);

    const void* pBVHMem = deserializer.map(m_bvhSerializeAllocation);
    WiVeBVH8Build8<OfflineBVHLeaf> bvh { pandora::serialization::GetWiVeBVH8(pBVHMem), gsl::span(pLeafs, m_numLeafs) };
    m_bvh.emplace(std::move(bvh));

    g_stats.memory.botLevelLoaded += m_bvh->sizeBytes();
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

pandora::LRUBVHSceneCache::LRUBVHSceneCache(gsl::span<const SubScene*> subScenes, tasking::LRUCacheTS* pSceneCache, size_t maxSize)
{
    //spdlog::warn("Using in-memory serializer for BVH cache");
    //auto pSerializer = std::make_unique<tasking::InMemorySerializer>();
    auto pSerializer = std::make_unique<tasking::SplitFileSerializer>("pandora_render_bvh", 512 * 1024 * 1024, mio_cache_control::cache_mode::no_buffering);

    using CacheBuilder = tasking::LRUCacheTS::Builder;
    CacheBuilder cacheBuilder = CacheBuilder(std::move(pSerializer));

    for (const SubScene* pSubScene : subScenes) {
        const CachedBVH* pBVH = createBVH(pSubScene, pSceneCache, &cacheBuilder);

        std::unordered_set<const SceneNode*> childNodes;
        std::function<void(const SceneNode*)> collectSceneNodesRecurse = [&](const SceneNode* pNode) {
            childNodes.insert(pNode);

            for (const auto& [pChild, _] : pNode->children)
                collectSceneNodesRecurse(pChild.get());
        };
        for (const auto& [pNode, _] : pSubScene->sceneNodes)
            collectSceneNodesRecurse(pNode);

        std::vector<CachedBVH*> childBVHs;
        std::transform(std::begin(childNodes), std::end(childNodes), std::back_inserter(childBVHs),
            [this](const SceneNode* pNode) {
                auto iter = m_bvhSceneLUT.find(pNode);
                ALWAYS_ASSERT(iter != std::end(m_bvhSceneLUT));
                return iter->second.get();
            });
        m_childBVHs[pSubScene] = std::move(childBVHs);
    }

    m_lruCache = cacheBuilder.build(maxSize);
}

CachedBVHSubScene LRUBVHSceneCache::fromSubScene(const SubScene* pSubScene)
{
    auto stopWatch = g_stats.timings.botLevelBuildTime.getScopedStopwatch();

    // Load main BVH
    ALWAYS_ASSERT(m_bvhSceneLUT.find(static_cast<const void*>(pSubScene)) != std::end(m_bvhSceneLUT));
    auto pCachedBVH = m_lruCache->makeResident(m_bvhSceneLUT.find(static_cast<const void*>(pSubScene))->second.get());

    // Load child BVHs (BVHs referred to by the leafs of the main BVH)
    ALWAYS_ASSERT(m_childBVHs.find(pSubScene) != std::end(m_childBVHs));

    const auto& childBVHs = m_childBVHs.find(pSubScene)->second;
    std::vector<tasking::CachedPtr<CachedBVH>> cachedChildBVHs;
    std::transform(std::begin(childBVHs), std::end(childBVHs), std::back_inserter(cachedChildBVHs),
        [this](CachedBVH* pCachedBVH) {
            return m_lruCache->makeResident<CachedBVH>(pCachedBVH);
        });

    return CachedBVHSubScene(std::move(pCachedBVH), std::move(cachedChildBVHs));
}

CachedBVH* LRUBVHSceneCache::createBVH(const SceneNode* pSceneNode, tasking::LRUCacheTS* pGeomCache, tasking::CacheBuilder* pCacheBuilder)
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
            const Transform transform { *optTransform };
            transform.transformToWorld(childBounds);
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

    // Construct BVH
    WiVeBVH8Build8<OfflineBVHLeaf> bvh { leafs };

    // Store Evictable wrapper class
    auto pCachedBVHOwner = std::make_unique<CachedBVH>(std::move(bvh), bounds);
    auto* pCachedBVH = pCachedBVHOwner.get();

    // Add Evictable BVH to cache
    pCacheBuilder->registerCacheable(pCachedBVH, true);

    // Store SubScene pointer to Evictable BVH in a look-up table
    m_bvhSceneLUT[static_cast<const void*>(pSceneNode)] = std::move(pCachedBVHOwner);

    return pCachedBVH;
}

CachedBVH* LRUBVHSceneCache::createBVH(const SubScene* pSubScene, tasking::LRUCacheTS* pGeomCache, tasking::CacheBuilder* pCacheBuilder)
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
            const Transform transform { *optTransform };
            transform.transformToWorld(childBounds);
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

    // Construct BVH
    WiVeBVH8Build8<OfflineBVHLeaf> bvh { leafs };

    // Store Evictable wrapper class
    auto pCachedBVHOwner = std::make_unique<CachedBVH>(std::move(bvh), bounds);
    auto pCachedBVH = pCachedBVHOwner.get();

    // Add Evictable BVH to cache
    pCacheBuilder->registerCacheable(pCachedBVH, true);

    // Store SubScene pointer to Evictable BVH in a look-up table
    m_bvhSceneLUT[static_cast<const void*>(pSubScene)] = std::move(pCachedBVHOwner);

    return pCachedBVH;
}

}