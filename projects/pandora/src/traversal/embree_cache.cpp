#include "pandora/traversal/embree_cache.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/utility/enumerate.h"
#include <glm/gtc/type_ptr.hpp>
#include <optick.h>
#include <spdlog/spdlog.h>
#include <tbb/task_arena.h>

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
{
    switch (code) {
    case RTC_ERROR_NONE:
        spdlog::error("In EmbreeCache: RTC_ERROR_NONE {}", str);
        break;
    case RTC_ERROR_UNKNOWN:
        spdlog::error("In EmbreeCache: RTC_ERROR_UNKNOWN {}", str);
        break;
    case RTC_ERROR_INVALID_ARGUMENT:
        spdlog::error("In EmbreeCache: RTC_ERROR_INVALID_ARGUMENT {}", str);
        break;
    case RTC_ERROR_INVALID_OPERATION:
        spdlog::error("In EmbreeCache: RTC_ERROR_INVALID_OPERATION {}", str);
        break;
    case RTC_ERROR_OUT_OF_MEMORY:
        spdlog::error("In EmbreeCache: RTC_ERROR_OUT_OF_MEMORY {}", str);
        break;
    case RTC_ERROR_UNSUPPORTED_CPU:
        spdlog::error("In EmbreeCache: RTC_ERROR_UNSUPPORTED_CPU {}", str);
        break;
    case RTC_ERROR_CANCELLED:
        spdlog::error("In EmbreeCache: RTC_ERROR_CANCELLED {}", str);
        break;
    }
}

namespace pandora {

Bounds SubScene::computeBounds() const
{
    Bounds bounds;
    for (const auto& pSceneObject : sceneObjects) {
        bounds.extend(pSceneObject->pShape->getBounds());
    }

    for (const auto& childAndTransform : sceneNodes) {
        const auto& [pChild, transformOpt] = childAndTransform;
        Bounds childBounds = pChild->computeBounds();
        if (transformOpt)
            childBounds *= transformOpt.value();
        bounds.extend(childBounds);
    }
    return bounds;
}

CachedEmbreeScene::CachedEmbreeScene(RTCScene scene)
    : scene(scene)
{
}

CachedEmbreeScene::~CachedEmbreeScene()
{
    // TODO: delete additional user data because we're leaking memory right now...
    rtcReleaseScene(scene);
}

LRUEmbreeSceneCache::LRUEmbreeSceneCache(size_t maxSize)
    : m_maxSize(maxSize)
    , m_embreeDevice(rtcNewDevice(nullptr))
{
    rtcSetDeviceErrorFunction(m_embreeDevice, embreeErrorFunc, nullptr);
    rtcSetDeviceMemoryMonitorFunction(m_embreeDevice, memoryMonitorCallback, this);
}

LRUEmbreeSceneCache::~LRUEmbreeSceneCache()
{
    spdlog::info("~LRUEmbreeSceneCache(): memory usage = {} bytes", m_size.load());

    rtcReleaseDevice(m_embreeDevice);
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::fromSceneObjectGroup(
    const void* pKey, gsl::span<const SceneObject*> sceneObjects)
{
    //    std::lock_guard l { m_mutex };
    auto l = tryLock();

    // NOTE: run in task arena to prevent deadlocks (or crashes on Windows). Embree uses TBB in the BVH builders
    //  which means that the TBB task scheduler is invoked while we're holding a lock. This means that another
    //  of our scheduler/worker tasks may get executed during the BVH construction. Such a task may also want to
    //  build an Embree BVH so it will enter this function and request for the lock (the thread would be requesting
    //  a lock from the same thread (but different task)). The TBB task arena was designed to prevent this issue by
    //  only allowing tasks to be run that were specified within the arena (so only Embree builder tasks).
    tbb::task_arena ta;
    return ta.execute([&]() {
        if (auto lutIter = m_lookUp.find(pKey); lutIter != std::end(m_lookUp)) {
            // Remove from list and add to end
            auto& listIter = lutIter->second;
            m_scenes.splice(std::end(m_scenes), m_scenes, listIter);
            listIter = --std::end(m_scenes);

            return listIter->scene;
        } else {
            auto sizeBefore = m_size.load();
            auto embreeScene = createEmbreeScene(sceneObjects);
            auto sizeAfter = m_size.load();
            //spdlog::info("Created Embree BVH of {} bytes", sizeAfter - sizeBefore);

            if (m_size.load() > m_maxSize)
                evict();

            m_scenes.emplace_back(CacheItem { pKey, embreeScene });
            auto listIter = --std::end(m_scenes);
            m_lookUp[pKey] = listIter;

            return listIter->scene;
        }
    });
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::createEmbreeScene(gsl::span<const SceneObject*> sceneObjects)
{
    OPTICK_EVENT();
    RTCScene embreeScene = rtcNewScene(m_embreeDevice);
    rtcSetSceneFlags(embreeScene, RTC_SCENE_FLAG_COMPACT);

    for (const auto& pSceneObject : sceneObjects) {
        Shape* pShape = pSceneObject->pShape.get();

        RTCGeometry embreeGeometry = pShape->createEvictSafeEmbreeGeometry(m_embreeDevice, pSceneObject);
        rtcCommitGeometry(embreeGeometry);

        rtcAttachGeometry(embreeScene, embreeGeometry);
        rtcReleaseGeometry(embreeGeometry); // Decrement reference counter (scene will keep it alive)
    }

    shareCommitScene(embreeScene);
    rtcCommitScene(embreeScene);
    return std::make_shared<CachedEmbreeScene>(embreeScene);
}

void LRUEmbreeSceneCache::shareCommitScene(RTCScene scene)
{
    std::lock_guard l { m_scenesBeingCommitedLock };
    m_scenesBeingCommited.push_back(scene);
}

std::unique_lock<std::mutex> LRUEmbreeSceneCache::tryLock()
{
    std::unique_lock<std::mutex> l { m_mutex, std::defer_lock };
    while (!l.try_lock()) {
        RTCScene sceneToCommit = nullptr;
        {
            std::lock_guard l2 { m_scenesBeingCommitedLock };
            if (m_scenesBeingCommited.size() > 0)
                sceneToCommit = m_scenesBeingCommited.front();
        }
        if (sceneToCommit)
            rtcCommitScene(sceneToCommit);
    }
    return l;
}

void LRUEmbreeSceneCache::evict()
{
    spdlog::info("LRUEmbreeSceneCache::evict");
    for (auto iter = std::begin(m_scenes); iter != std::end(m_scenes);) {
        // If a scene is still actively being used then there is no point in removing it
        if (iter->scene.use_count() > 1) {
            iter++;
            continue;
        }

        m_lookUp.erase(iter->pKey);
        iter = m_scenes.erase(iter); // Make iter point to the next item (calling iter++ after erase will reference free'd memory)

        if (m_size.load() < m_maxSize * 3 / 4)
            break;
    }
    spdlog::info("Size of BVHs after evict: {}", m_size.load());
}

bool LRUEmbreeSceneCache::memoryMonitorCallback(void* pThisMem, ssize_t bytes, bool post)
{
    auto* pThis = reinterpret_cast<LRUEmbreeSceneCache*>(pThisMem);

    if (bytes >= 0) {
        pThis->m_size.fetch_add(bytes);
        // Not perfect: also counts temporary allocations
        g_stats.memory.botLevelLoaded += bytes;
    } else {
        pThis->m_size.fetch_sub(-bytes);
        g_stats.memory.botLevelEvicted += -bytes;
    }

    return true;
}

int LRUEmbreeSceneCache::computeMaxInstanceDepthRecurse(const SceneNode* pSceneNode, int depth)
{
    int maxDepth = depth;
    for (const auto& [pChildNode, _] : pSceneNode->children) {
        maxDepth = std::max(maxDepth, computeMaxInstanceDepthRecurse(pChildNode.get(), depth + 1));
    }
    return maxDepth;
}

}