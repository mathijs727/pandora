#include "pandora/traversal/embree_acceleration_structure.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/utility/enumerate.h"
#include "pandora/utility/error_handling.h"
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <stack>
#include <unordered_map>
#include <unordered_set>

namespace pandora {

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
{
    switch (code) {
    case RTC_ERROR_NONE:
        spdlog::error("RTC_ERROR_NONE {}", str);
        break;
    case RTC_ERROR_UNKNOWN:
        spdlog::error("RTC_ERROR_UNKNOWN {}", str);
        break;
    case RTC_ERROR_INVALID_ARGUMENT:
        spdlog::error("RTC_ERROR_INVALID_ARGUMENT {}", str);
        break;
    case RTC_ERROR_INVALID_OPERATION:
        spdlog::error("RTC_ERROR_INVALID_OPERATION {}", str);
        break;
    case RTC_ERROR_OUT_OF_MEMORY:
        spdlog::error("RTC_ERROR_OUT_OF_MEMORY {}", str);
        break;
    case RTC_ERROR_UNSUPPORTED_CPU:
        spdlog::error("RTC_ERROR_UNSUPPORTED_CPU {}", str);
        break;
    case RTC_ERROR_CANCELLED:
        spdlog::error("RTC_ERROR_CANCELLED {}", str);
        break;
    }
}

EmbreeAccelerationStructureBuilder::EmbreeAccelerationStructureBuilder(const Scene& scene, tasking::TaskGraph* pTaskGraph)
    : m_pTaskGraph(pTaskGraph)
{
    m_embreeDevice = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_embreeDevice, embreeErrorFunc, nullptr);

    spdlog::info("Verifying instance depth is supported by Embree build (RTC_MAX_INSTANCE_LEVEL_COUNT)");
    verifyInstanceDepth(scene.pRoot.get());

    spdlog::info("Starting BVH build");
    m_sceneCache.clear();
    m_embreeScene = buildRecurse(scene.pRoot.get());
    spdlog::info("Finsihed building BVH");
}

RTCScene EmbreeAccelerationStructureBuilder::buildRecurse(const SceneNode* pSceneNode)
{
    RTCScene embreeScene = rtcNewScene(m_embreeDevice);

    // Offset geomID by 1 so that we never have geometry with ID=0. This way we know that if hit.instID[x] = 0
    // then this means that the value is invalid (since Embree always sets it to 0 when invalid instead of
	//  RTC_INVALID_GEOMETRY_ID).
    unsigned geometryID = 1;
    for (const auto& pSceneObject : pSceneNode->objects) {
        const Shape* pShape = pSceneObject->pShape.get();
        RTCGeometry embreeGeometry = pShape->createEmbreeGeometry(m_embreeDevice);
        rtcSetGeometryUserData(embreeGeometry, pSceneObject.get());
        rtcCommitGeometry(embreeGeometry);

        rtcAttachGeometryByID(embreeScene, embreeGeometry, geometryID++);
    }

    for (const auto& [pChildNode, optTransform] : pSceneNode->children) {
        RTCScene childScene;
        if (auto iter = m_sceneCache.find(pChildNode.get()); iter != std::end(m_sceneCache)) {
            childScene = iter->second;
        } else {
            childScene = buildRecurse(pChildNode.get());
            m_sceneCache[pChildNode.get()] = childScene;
        }

        RTCGeometry embreeInstanceGeometry = rtcNewGeometry(m_embreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(embreeInstanceGeometry, childScene);
        rtcSetGeometryUserData(embreeInstanceGeometry, childScene);
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
    return embreeScene;
}

void EmbreeAccelerationStructureBuilder::verifyInstanceDepth(const SceneNode* pSceneNode, int depth)
{
    for (const auto& [pChildNode, optTransform] : pSceneNode->children) {
        verifyInstanceDepth(pChildNode.get(), depth + 1);
    }
    ALWAYS_ASSERT(depth <= RTC_MAX_INSTANCE_LEVEL_COUNT);
}
}