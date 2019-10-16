#include "pandora/traversal/embree_acceleration_structure.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
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

    spdlog::info("Starting BVH build");
    m_instances.clear();
    m_embreeScene = buildRecurse(scene.root.get(), {});
    spdlog::info("Finsihed building BVH");
}

RTCScene EmbreeAccelerationStructureBuilder::buildRecurse(const SceneNode* pSceneNode, std::optional<RTCScene> parentScene)
{
    RTCScene embreeScene;
    if (auto iter = m_instances.find(pSceneNode); iter != std::end(m_instances)) {
        embreeScene = iter->second;
    } else {
        embreeScene = rtcNewScene(m_embreeDevice);
        for (const auto& pSceneObject : pSceneNode->objects) {
            const Shape* pShape = pSceneObject->pShape.get();
            const IntersectGeometry* pIntersectGeometry = pShape->getIntersectGeometry();
            RTCGeometry embreeGeometry = pIntersectGeometry->createEmbreeGeometry(m_embreeDevice);
            rtcSetGeometryUserData(embreeGeometry, pSceneObject.get());
            rtcCommitGeometry(embreeGeometry);

            unsigned geometryID = rtcAttachGeometry(embreeScene, embreeGeometry);
            (void)geometryID;
        }

		m_instances[pSceneNode] = embreeScene;
    }

    for (const auto& child : pSceneNode->children)
        buildRecurse(child.get(), embreeScene);

    rtcCommitScene(embreeScene);

    const bool isRootNode = !parentScene;
    if (isRootNode) {
        ALWAYS_ASSERT(!pSceneNode->transform);
        m_embreeScene = embreeScene;
    } else {
        //spdlog::info("Creating instanced Embree geometry");
        RTCGeometry embreeInstanceGeometry = rtcNewGeometry(m_embreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(embreeInstanceGeometry, embreeScene);
        if (pSceneNode->transform)
            rtcSetGeometryTransform(
                embreeInstanceGeometry, 0,
                RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
                glm::value_ptr(*pSceneNode->transform));
        rtcCommitGeometry(embreeInstanceGeometry);

        unsigned geometryID = rtcAttachGeometry(*parentScene, embreeInstanceGeometry);
        (void)geometryID;
    }

    return embreeScene;
}

}