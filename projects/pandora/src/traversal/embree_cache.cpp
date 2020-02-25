#include "pandora/traversal/embree_cache.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/utility/enumerate.h"
#include <glm/gtc/type_ptr.hpp>
#include <optick.h>
#include <spdlog/spdlog.h>

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

CachedEmbreeScene::CachedEmbreeScene(RTCScene scene, std::vector<std::shared_ptr<CachedEmbreeScene>>&& childrenScenes)
    : scene(scene)
    , childrenScenes(std::move(childrenScenes))
{
}

CachedEmbreeScene::~CachedEmbreeScene()
{
    // TODO: delete additional user data because we're leaking memory right now...
    rtcReleaseScene(scene);
    childrenScenes.clear();
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
    rtcReleaseDevice(m_embreeDevice);
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::fromSceneNode(const SceneNode* pSceneNode)
{
    const void* pKey = pSceneNode;
    if (auto lutIter = m_lookUp.find(pKey); lutIter != std::end(m_lookUp)) {
        // Item is used => update LRU
        auto& listIter = lutIter->second;
        m_scenes.splice(std::end(m_scenes), m_scenes, listIter);
        listIter = --std::end(m_scenes);

        return listIter->scene;
    } else {
        m_scenes.emplace_back(CacheItem { pKey, createEmbreeScene(pSceneNode) });
        auto listIter = --std::end(m_scenes);
        m_lookUp[pKey] = listIter;

        return listIter->scene;
    }
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::fromSubScene(const SubScene* pSubScene)
{
    const void* pKey = pSubScene;
    if (auto lutIter = m_lookUp.find(pKey); lutIter != std::end(m_lookUp)) {
        // Remove from list and add to end
        auto& listIter = lutIter->second;
        m_scenes.splice(std::end(m_scenes), m_scenes, listIter);
        listIter = --std::end(m_scenes);

        return listIter->scene;
    } else {
        auto sizeBefore = m_size.load();
        auto embreeScene = createEmbreeScene(pSubScene);
        auto sizeAfter = m_size.load();
        //spdlog::info("Created Embree BVH of {} bytes", sizeAfter - sizeBefore);

        if (m_size.load() > m_maxSize)
            evict();

        m_scenes.emplace_back(CacheItem { pKey, embreeScene });
        auto listIter = --std::end(m_scenes);
        m_lookUp[pKey] = listIter;

        return listIter->scene;
    }
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::createEmbreeScene(const SceneNode* pSceneNode)
{
    OPTICK_EVENT();
    RTCScene embreeScene = rtcNewScene(m_embreeDevice);
    rtcSetSceneFlags(embreeScene, RTC_SCENE_FLAG_COMPACT);

    for (const auto& pSceneObject : pSceneNode->objects) {
        Shape* pShape = pSceneObject->pShape.get();

        RTCGeometry embreeGeometry = pShape->createEvictSafeEmbreeGeometry(m_embreeDevice, pSceneObject.get());
        rtcCommitGeometry(embreeGeometry);

        rtcAttachGeometry(embreeScene, embreeGeometry);
        rtcReleaseGeometry(embreeGeometry); // Decrement reference counter (scene will keep it alive)
    }

    std::vector<std::shared_ptr<CachedEmbreeScene>> children;
    for (const auto& [pChildNode, optTransform] : pSceneNode->children) {
        std::shared_ptr<CachedEmbreeScene> pChildScene = fromSceneNode(pSceneNode);
        children.push_back(pChildScene);

        RTCGeometry embreeInstanceGeometry = rtcNewGeometry(m_embreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(embreeInstanceGeometry, pChildScene->scene);
        rtcSetGeometryUserData(embreeInstanceGeometry, pChildScene->scene);
        if (optTransform) {
            rtcSetGeometryTransform(
                embreeInstanceGeometry, 0,
                RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
                glm::value_ptr(*optTransform));
        } else {
            glm::mat4 identityMatrix = glm::identity<glm::mat4>();
            rtcSetGeometryTransform(
                embreeInstanceGeometry, 0,
                RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
                glm::value_ptr(identityMatrix));
        }
        rtcCommitGeometry(embreeInstanceGeometry);
        rtcAttachGeometry(embreeScene, embreeInstanceGeometry);
        rtcReleaseGeometry(embreeInstanceGeometry); // Decrement reference counter (scene will keep it alive)
    }

    rtcCommitScene(embreeScene);
    return std::make_shared<CachedEmbreeScene>(embreeScene, std::move(children));
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::createEmbreeScene(const SubScene* pSubScene)
{
    OPTICK_EVENT();

#ifndef NDEBUG
    int instanceLevels = 0;
    for (const auto& [pSceneNode, _] : pSubScene->sceneNodes) {
        instanceLevels = std::max(instanceLevels, computeMaxInstanceDepthRecurse(pSceneNode, 1));
    }
    if (instanceLevels > RTC_MAX_INSTANCE_LEVEL_COUNT) {
        spdlog::critical("Embree does not support instancing {} levels deep (only {} levels are supported)", instanceLevels, RTC_MAX_INSTANCE_LEVEL_COUNT);
        throw std::runtime_error("Too many instance levels");
    }
#endif

    auto stopWatch = g_stats.timings.botLevelBuildTime.getScopedStopwatch();

    RTCScene embreeScene = rtcNewScene(m_embreeDevice);
    rtcSetSceneFlags(embreeScene, RTC_SCENE_FLAG_COMPACT);

    for (const auto& pSceneObject : pSubScene->sceneObjects) {
        const Shape* pShape = pSceneObject->pShape.get();

        RTCGeometry embreeGeometry = pShape->createEvictSafeEmbreeGeometry(m_embreeDevice, pSceneObject);
        rtcCommitGeometry(embreeGeometry);

        rtcAttachGeometry(embreeScene, embreeGeometry);
        rtcReleaseGeometry(embreeGeometry); // Decrement reference counter (scene will keep it alive)
    }

    std::vector<std::shared_ptr<CachedEmbreeScene>> children;
    for (const auto&& [geomID, childAndTransform] : enumerate(pSubScene->sceneNodes)) {
        const auto& [pChildNode, optTransform] = childAndTransform;

        std::shared_ptr<CachedEmbreeScene> pChildScene = fromSceneNode(pChildNode);
        children.push_back(pChildScene);

        RTCGeometry embreeInstanceGeometry = rtcNewGeometry(m_embreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(embreeInstanceGeometry, pChildScene->scene);
        rtcSetGeometryUserData(embreeInstanceGeometry, pChildScene->scene);
        if (optTransform) {
            rtcSetGeometryTransform(
                embreeInstanceGeometry, 0,
                RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
                glm::value_ptr(*optTransform));
        } else {
            glm::mat4 identityMatrix = glm::identity<glm::mat4>();
            rtcSetGeometryTransform(
                embreeInstanceGeometry, 0,
                RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
                glm::value_ptr(identityMatrix));
        }
        rtcCommitGeometry(embreeInstanceGeometry);

        rtcAttachGeometry(embreeScene, embreeInstanceGeometry);
        rtcReleaseGeometry(embreeInstanceGeometry); // Decrement reference counter (scene will keep it alive)
    }

    rtcCommitScene(embreeScene);
    return std::make_shared<CachedEmbreeScene>(embreeScene, std::move(children));
}

void LRUEmbreeSceneCache::evict()
{
    spdlog::debug("LRUEmbreeSceneCache::evict");
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
    spdlog::debug("Size of BVHs after evict: {}", m_size.load());
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