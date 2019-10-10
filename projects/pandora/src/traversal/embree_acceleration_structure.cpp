#include "pandora/traversal/embree_acceleration_structure.h"
#include "pandora/geometry/triangle.h"
#include "pandora/graphics_core/scene.h"
#include <spdlog/spdlog.h>
#include <stack>

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

    m_embreeScene = rtcNewScene(m_embreeDevice);

    std::stack<const SceneNode*> traversalStack;
    traversalStack.push(&scene.m_root);
    while (!traversalStack.empty()) {
        const SceneNode* pSceneNode = traversalStack.top();
        traversalStack.pop();

        // TODO: handle instancing!
        for (const auto& pSceneObject : pSceneNode->objects) {
            const TriangleMesh* pTriangleMesh = pSceneObject->pGeometry.get();
            RTCGeometry embreeGeometry = pTriangleMesh->createEmbreeGeometry(m_embreeDevice);
            rtcSetGeometryUserData(embreeGeometry, pSceneObject.get());
            rtcCommitGeometry(embreeGeometry);

            unsigned geometryID = rtcAttachGeometry(m_embreeScene, embreeGeometry);
            (void)geometryID;
        }

        // TODO: handle instancing!
        for (const auto& child : pSceneNode->children)
            traversalStack.push(child.get());
    }

    rtcCommitScene(m_embreeScene);
}

}