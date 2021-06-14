#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/sub_scene.h"
#include <glm/glm.hpp>
#include <span>
#include <memory>
#include <optional>
#include <stream/cache/cached_ptr.h>
#include <stream/cache/lru_cache_ts.h>
#include <unordered_map>
#include <variant>

namespace pandora {

struct CachedBVH;

struct OfflineBVHLeaf {
public:
    OfflineBVHLeaf(const SceneObject* pSceneObject, uint32_t primID);
    OfflineBVHLeaf(const CachedBVH* pBVHScene, const glm::mat4& localToWorldTransform);
    OfflineBVHLeaf(const CachedBVH* pBVHScene);

    Bounds getBounds() const;
    bool intersect(Ray& ray, SurfaceInteraction& si) const;
    bool intersectAny(Ray& ray) const;

public:
    struct Primitive {
        const SceneObject* pSceneObject;
        const Shape* pShape;
        uint32_t primID;
    };
    struct Instance {
        const CachedBVH* pCachedBVH;
        std::optional<glm::mat4> transform;
    };
    std::variant<Primitive, Instance> m_object;
};

struct CachedBVH : public tasking::Evictable {
public:
    CachedBVH(WiVeBVH8Build8<OfflineBVHLeaf>&& bvh, const Bounds& bounds);
    CachedBVH(CachedBVH&&) = default;
    ~CachedBVH() override = default;

    Bounds getBounds() const;
    bool intersect(Ray& ray, SurfaceInteraction& si) const;
    bool intersectAny(Ray& ray) const;

    size_t sizeBytes() const override;
    void serialize(tasking::Serializer& serializer) override;

private:
    void doEvict() override;
    void doMakeResident(tasking::Deserializer& deserializer) override;

private:
    std::optional<WiVeBVH8Build8<OfflineBVHLeaf>> m_bvh;
    Bounds m_bounds;

    tasking::Allocation m_bvhSerializeAllocation;
    tasking::Allocation m_leafsSerializeAllocation;
    size_t m_numLeafs;
};

class LRUBVHSceneCache;
class CachedBVHSubScene {
public:
    bool intersect(Ray& ray, SurfaceInteraction& si) const;
    bool intersectAny(Ray& ray) const;

private:
    friend class LRUBVHSceneCache;
    CachedBVHSubScene(tasking::CachedPtr<CachedBVH>&& pBVH, std::vector<tasking::CachedPtr<CachedBVH>>&& childBVHs);

private:
    tasking::CachedPtr<CachedBVH> m_pBVH;
    std::vector<tasking::CachedPtr<CachedBVH>> m_childBVHs;
};

class LRUBVHSceneCache {
public:
    LRUBVHSceneCache(std::span<const SubScene*> subScenes, tasking::LRUCacheTS* pSceneCache, size_t maxSize);

    CachedBVHSubScene fromSubScene(const SubScene* pSubScene);

private:
    CachedBVH* createBVH(const SceneNode* pSceneNode, tasking::LRUCacheTS* pGeomCache, tasking::CacheBuilder* pCacheBuilder);
    CachedBVH* createBVH(const SubScene* pSubScene, tasking::LRUCacheTS* pGeomCache, tasking::CacheBuilder* pCacheBuilder);

private:
    std::unordered_map<const void*, std::unique_ptr<CachedBVH>> m_bvhSceneLUT;
    std::unordered_map<const SubScene*, std::vector<CachedBVH*>> m_childBVHs;

    std::optional<tasking::LRUCacheTS> m_lruCache;
};

}