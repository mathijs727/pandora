#include "pandora/traversal/embree_acceleration_structure.h"
#include "pandora/geometry/triangle.h"
#include "pandora/graphics_core/scene.h"
#include <stack>

namespace pandora {

EmbreeAccelerationStructureBuilder::EmbreeAccelerationStructureBuilder(const Scene& scene)
{
    m_embreeDevice = rtcNewDevice(nullptr);
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
}

}