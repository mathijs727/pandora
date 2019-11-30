#include "pandora/traversal/embree_cache.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/utility/enumerate.h"
#include <glm/gtc/type_ptr.hpp>
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

CachedEmbreeScene::CachedEmbreeScene(RTCScene scene, std::vector<std::shared_ptr<CachedEmbreeScene>>&& parents)
    : scene(scene)
    , parents(std::move(parents))
{
}

CachedEmbreeScene::~CachedEmbreeScene()
{
    rtcReleaseScene(scene);
}

LRUEmbreeSceneCache::LRUEmbreeSceneCache(size_t maxSize)
    : m_maxSize(maxSize)
    , m_embreeDevice(rtcNewDevice(nullptr))
{
    rtcSetDeviceErrorFunction(m_embreeDevice, embreeErrorFunc, nullptr);
    rtcSetDeviceMemoryMonitorFunction(m_embreeDevice, memoryMonitorCallback, this);
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::fromSceneNode(const SceneNode* pSceneNode)
{
    const void* pKey = pSceneNode;
    if (auto iter = m_lookUp.find(pKey); iter != std::end(m_lookUp)) {
        return iter->second->scene;
    } else {
        m_scenes.emplace_back(CacheItem { pKey, createEmbreeScene(pSceneNode) });
        auto listIter = --std::end(m_scenes);
        m_lookUp[pKey] = listIter;

        auto pScene = listIter->scene;
        if (m_size.load() > m_maxSize)
            evict();

        return pScene;
    }
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::fromSubScene(const SubScene* pSubScene)
{
    const void* pKey = pSubScene;
    if (auto iter = m_lookUp.find(pKey); iter != std::end(m_lookUp)) {
        return iter->second->scene;
    } else {
        m_scenes.emplace_back(CacheItem { pKey, createEmbreeScene(pSubScene) });
        auto listIter = --std::end(m_scenes);
        m_lookUp[pKey] = listIter;

        auto pScene = listIter->scene;
        if (m_size.load() > m_maxSize)
            evict();

        return pScene;
    }
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::createEmbreeScene(const SceneNode* pSceneNode)
{

    RTCScene embreeScene = rtcNewScene(m_embreeDevice);

    // Offset geomID by 1 so that we never have geometry with ID=0. This way we know that if hit.instID[x] = 0
    // then this means that the value is invalid (since Embree always sets it to 0 when invalid instead of
    //  RTC_INVALID_GEOMETRY_ID).
    unsigned geometryID = 1;
    for (const auto& pSceneObject : pSceneNode->objects) {
        Shape* pShape = pSceneObject->pShape.get();

        RTCGeometry embreeGeometry = pShape->createEmbreeGeometry(m_embreeDevice);
        rtcSetGeometryUserData(embreeGeometry, pSceneObject.get());
        rtcCommitGeometry(embreeGeometry);

        rtcAttachGeometryByID(embreeScene, embreeGeometry, geometryID++);
    }

    std::vector<std::shared_ptr<CachedEmbreeScene>> children;
    for (const auto& [pChildNode, optTransform] : pSceneNode->children) {
        std::shared_ptr<CachedEmbreeScene> pChildScene = fromSceneNode(pSceneNode);

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
        // Offset geomID by 1 so that we never have geometry with ID=0. This way we know that if hit.instID[x] = 0
        // then this means that the value is invalid (since Embree always sets it to 0 when invalid instead of RTC_INVALID_GEOMETRY_ID).
        rtcAttachGeometryByID(embreeScene, embreeInstanceGeometry, geometryID++);
    }

    rtcCommitScene(embreeScene);

    return std::make_shared<CachedEmbreeScene>(embreeScene, std::move(children));
}

std::shared_ptr<CachedEmbreeScene> LRUEmbreeSceneCache::createEmbreeScene(const SubScene* pSubScene)
{
    RTCScene embreeScene = rtcNewScene(m_embreeDevice);

	// Offset geomID by 1 so that we never have geometry with ID=0. This way we know that if hit.instID[x] = 0
    // then this means that the value is invalid (since Embree always sets it to 0 when invalid instead of
    //  RTC_INVALID_GEOMETRY_ID).
    unsigned geometryID = 1;
    for (const auto& pSceneObject : pSubScene->sceneObjects) {
        const Shape* pShape = pSceneObject->pShape.get();

        RTCGeometry embreeGeometry = pShape->createEmbreeGeometry(m_embreeDevice);
        rtcSetGeometryUserData(embreeGeometry, pSceneObject);
        rtcCommitGeometry(embreeGeometry);

        rtcAttachGeometryByID(embreeScene, embreeGeometry, geometryID++);
    }

    std::vector<std::shared_ptr<CachedEmbreeScene>> children;
    for (const auto&& [geomID, childAndTransform] : enumerate(pSubScene->sceneNodes)) {
        const auto& [pChildNode, optTransform] = childAndTransform;

        std::shared_ptr<CachedEmbreeScene> pChildScene = fromSceneNode(pChildNode);

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

        rtcAttachGeometryByID(embreeScene, embreeInstanceGeometry, geometryID++);
    }

    rtcCommitScene(embreeScene);
    return std::make_shared<CachedEmbreeScene>(embreeScene, std::move(children));
}

void LRUEmbreeSceneCache::evict()
{
    for (auto iter = std::begin(m_scenes); iter != std::end(m_scenes); iter++) {
        spdlog::debug("Evicting BVH");
        m_lookUp.erase(iter->pKey);
        m_scenes.erase(iter);

        if (m_size.load() < m_maxSize)
            break;
    }
}

bool LRUEmbreeSceneCache::memoryMonitorCallback(void* pThisMem, ssize_t bytes, bool post)
{
    auto* pThis = reinterpret_cast<LRUEmbreeSceneCache*>(pThisMem);

    if (bytes > 0)
        pThis->m_size.fetch_add(bytes);
    else
        pThis->m_size.fetch_sub(-bytes);

    return true;
}
}