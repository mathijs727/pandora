#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/sub_scene.h"
#include <memory>
#include <stream/cache/lru_cache.h>
#include <unordered_map>
#include <variant>
#include <optional>

namespace pandora {

struct CachedBVHScene;

struct Leaf {
public:
    Bounds getBounds() const;
    bool intersect(Ray& ray, RayHit& rayHit) const;
    bool intersectAny(Ray& ray) const;

public:
    struct Primitive {
        Shape* pShape;
        uint32_t primID;
	};
    std::variant<Primitive, CachedBVHScene*> m_object;
};

struct CachedBVHScene : public tasking::Evictable {
public:
    CachedBVHScene(WiVeBVH8Build8<Leaf>&& bvh, std::vector<Leaf>&& leafs);
    CachedBVHScene(CachedBVHScene&&) = default;
    ~CachedBVHScene() override = default;

	Bounds getBounds() const;
    bool intersect(Ray& ray, RayHit& rayHit) const;
    bool intersectAny(Ray& ray) const;

    size_t sizeBytes() const override;
    void serialize(tasking::Serializer& serializer) override;

private:
    void doEvict() override;
    void doMakeResident(tasking::Deserializer& deserializer) override;

private:
    std::optional<WiVeBVH8Build8<Leaf>> m_bvh;
    std::vector<Leaf> m_leafs;

	Bounds m_bounds;
};

struct LRUBVHSceneCache {
public:
    LRUBVHSceneCache(size_t maxSize);

    std::shared_ptr<CachedBVHScene> fromSubScene(const SubScene* pSubScene);

private:
    std::shared_ptr<CachedBVHScene> fromSceneNode(const SceneNode* pSceneNode);

    std::shared_ptr<CachedBVHScene> createEmbreeScene(const SceneNode* pSceneNode);
    std::shared_ptr<CachedBVHScene> createEmbreeScene(const SubScene* pSubScene);

    void evict();

private:
    std::unordered_map<const SubScene*, std::unique_ptr<CachedBVHScene>> m_subSceneLUT;
    tasking::LRUCache m_lruCache;
};

}