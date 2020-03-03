#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/traversal/sub_scene.h"
#include "stream/cache/lru_cache.h"
#include <atomic>
#include <embree3/rtcore.h>
#include <glm/mat4x4.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace pandora {

struct CachedEmbreeScene {
public:
    CachedEmbreeScene(RTCScene scene);
    CachedEmbreeScene(CachedEmbreeScene&&) = default;
    ~CachedEmbreeScene();

    RTCScene scene;
};

struct EmbreeSceneCache {
public:
    virtual std::shared_ptr<CachedEmbreeScene> fromSceneObjectGroup(const void* key, gsl::span<const SceneObject*> sceneObjects) = 0;
};

struct LRUEmbreeSceneCache : public EmbreeSceneCache {
public:
    LRUEmbreeSceneCache(size_t maxSize);
    ~LRUEmbreeSceneCache();

    std::shared_ptr<CachedEmbreeScene> fromSceneObjectGroup(const void* key, gsl::span<const SceneObject*> sceneObjects) override;

private:
    std::shared_ptr<CachedEmbreeScene> createEmbreeScene(gsl::span<const SceneObject*> sceneObjects);

    void evict();

    static bool memoryMonitorCallback(void* pThisMem, ssize_t bytes, bool post);
    static int computeMaxInstanceDepthRecurse(const SceneNode* pNode, int depth = 0);

private:
    const size_t m_maxSize;
    std::atomic_size_t m_size { 0 };

    std::mutex m_mutex;

    struct CacheItem {
        const void* pKey;
        std::shared_ptr<CachedEmbreeScene> scene;
    };
    std::list<CacheItem> m_scenes;
    std::unordered_map<const void*, std::list<CacheItem>::iterator> m_lookUp;

    RTCDevice m_embreeDevice;
};

}