#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"
#include "stream/cache/lru_cache.h"
#include <atomic>
#include <embree3/rtcore.h>
#include <glm/mat4x4.hpp>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace pandora {

struct SubScene {
    std::vector<std::pair<SceneNode*, std::optional<glm::mat4>>> sceneNodes;
    std::vector<SceneObject*> sceneObjects;

public:
    Bounds computeBounds() const;
};

struct CachedEmbreeScene {
public:
    CachedEmbreeScene(RTCScene scene, std::vector<std::shared_ptr<CachedEmbreeScene>>&& childrenScenes);
    CachedEmbreeScene(CachedEmbreeScene&&) = default;
    ~CachedEmbreeScene();

    RTCScene scene;

private:
    std::vector<std::shared_ptr<CachedEmbreeScene>> childrenScenes;
};

struct EmbreeSceneCache {
public:
    //virtual std::shared_ptr<CachedEmbreeScene> fromSceneNode(const SceneNode* pSceneNode) = 0;
    virtual std::shared_ptr<CachedEmbreeScene> fromSubScene(const SubScene* pSubScene) = 0;
};

struct LRUEmbreeSceneCache : public EmbreeSceneCache {
public:
    LRUEmbreeSceneCache(size_t maxSize);
    ~LRUEmbreeSceneCache();

    std::shared_ptr<CachedEmbreeScene> fromSubScene(const SubScene* pSubScene) override;

private:
    std::shared_ptr<CachedEmbreeScene> fromSceneNode(const SceneNode* pSceneNode);

    std::shared_ptr<CachedEmbreeScene> createEmbreeScene(const SceneNode* pSceneNode);
    std::shared_ptr<CachedEmbreeScene> createEmbreeScene(const SubScene* pSubScene);

    void evict();

    static bool memoryMonitorCallback(void* pThisMem, ssize_t bytes, bool post);
    static int computeMaxInstanceDepthRecurse(const SceneNode* pNode, int depth = 0);

private:
    const size_t m_maxSize;
    std::atomic_size_t m_size { 0 };

    struct CacheItem {
        const void* pKey;
        std::shared_ptr<CachedEmbreeScene> scene;
    };
    std::list<CacheItem> m_scenes;
    std::unordered_map<const void*, std::list<CacheItem>::iterator> m_lookUp;

    RTCDevice m_embreeDevice;
};

}