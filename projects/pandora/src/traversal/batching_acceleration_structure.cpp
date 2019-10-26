#include "pandora/traversal/batching_acceleration_structure.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include <spdlog/spdlog.h>
#include <vector>

namespace pandora {

static std::vector<TriangleShape> splitLargeTriangleShape(const TriangleShape& original);

BatchingAccelerationStructureBuilder::BatchingAccelerationStructureBuilder(stream::LRUCache* pcache, Scene* pScene, tasking::TaskGraph* pTaskGraph)
{
    // Split scene into smaller chunks
}

void BatchingAccelerationStructureBuilder::splitLargeSceneObjectsRecurse(SceneNode* pSceneNode, unsigned maxSize)
{
    RTCScene embreeScene = rtcNewScene(m_embreeDevice);
    std::vector<std::shared_ptr<SceneObject>> outObjects;
    for (const auto& pSceneObject : pSceneNode->objects) {
        // TODO
        Shape* pShape = pSceneObject->pShape.get();

        if (pShape->numPrimitives() > maxSize) {
            if (pSceneObject->pAreaLight) {
                spdlog::error("Shape attached to scene object with area light cannot be split");
                outObjects.push_back(pSceneObject);

            } else if (TriangleShape* pTriangleShape = dynamic_cast<TriangleShape*>(pShape)) {
                auto subShapes = splitLargeTriangleShape(*pTriangleShape);
                for (auto&& subShape : subShapes) {
                    auto pSubSceneObject = std::make_shared<SceneObject>(*pSceneObject);
                    pSubSceneObject->pShape = std::make_shared<TriangleShape>(std::move(subShape));
                    outObjects.push_back(pSubSceneObject);
                }
            } else {
                spdlog::error("Shape encountered with too many primitives but no way to split it");
                outObjects.push_back(pSceneObject);
            }
        } else {
            outObjects.push_back(pSceneObject);
        }
    }
    pSceneNode->objects = std::move(outObjects);

    for (auto childLink : pSceneNode->children) {
        auto&& [pChildNode, optTransform] = childLink;
        splitLargeSceneObjectsRecurse(pChildNode.get(), maxSize);
    }
}

RTCScene BatchingAccelerationStructureBuilder::buildRecurse(const SceneNode* pNode)
{
    return RTCScene();
}

void BatchingAccelerationStructureBuilder::verifyInstanceDepth(const SceneNode* pNode, int depth)
{
}

static std::vector<TriangleShape> splitLargeTriangleShape(const TriangleShape& original)
{
    return {};
}

}