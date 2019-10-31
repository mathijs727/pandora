#include "pandora/traversal/batching_acceleration_structure.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include "pandora/utility/enumerate.h"
#include "pandora/utility/error_handling.h"
#include <deque>
#include <embree3/rtcore.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_vector.h>
#include <unordered_map>
#include <vector>

namespace pandora {

static std::vector<std::shared_ptr<TriangleShape>> splitLargeTriangleShape(const TriangleShape& original, unsigned maxSize, RTCDevice embreeDevice);
static std::vector<std::shared_ptr<SceneNode>> createSubScenes(const Scene& scene, unsigned primitivesPerSubScene, RTCDevice embreeDevice);
static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str);

BatchingAccelerationStructureBuilder::BatchingAccelerationStructureBuilder(
    Scene* pScene,
    stream::LRUCache* pCache,
    tasking::TaskGraph* pTaskGraph,
    unsigned primitivesPerBatchingPoint)
    : m_pTaskGraph(pTaskGraph)
{
    m_embreeDevice = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_embreeDevice, embreeErrorFunc, nullptr);

    // NOTE: modifies pScene in place
    //
    // Split large shapes into smaller sub shpaes so we can guarantee that the batching poinst never exceed the given size.
    // This should also help with reducing the spatial extent of the batching points by (hopefully) splitting spatially large shapes.
    spdlog::info("Splitting large scene objects");
    splitLargeSceneObjectsRecurse(pScene->pRoot.get(), pCache, primitivesPerBatchingPoint / 2);

    spdlog::info("Splitting scene into sub scenes");
    m_subScenes = createSubScenes(*pScene, primitivesPerBatchingPoint, m_embreeDevice);
}

void BatchingAccelerationStructureBuilder::splitLargeSceneObjectsRecurse(SceneNode* pSceneNode, stream::LRUCache* pCache, unsigned maxSize)
{
    std::vector<std::shared_ptr<SceneObject>> outObjects;
    for (const auto& pSceneObject : pSceneNode->objects) {
        // TODO
        Shape* pShape = pSceneObject->pShape.get();

        if (pShape->numPrimitives() > maxSize) {
            auto pShapeOwner = pCache->makeResident(pShape);

            if (pSceneObject->pAreaLight) {
                spdlog::error("Shape attached to scene object with area light cannot be split");
                outObjects.push_back(pSceneObject);
            } else if (TriangleShape* pTriangleShape = dynamic_cast<TriangleShape*>(pShape)) {
                auto subShapes = splitLargeTriangleShape(*pTriangleShape, maxSize, m_embreeDevice);
                for (auto&& subShape : subShapes) {
                    auto pSubSceneObject = std::make_shared<SceneObject>(*pSceneObject);
                    pSubSceneObject->pShape = std::move(subShape);
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
        splitLargeSceneObjectsRecurse(pChildNode.get(), pCache, maxSize);
    }
}

static std::vector<std::shared_ptr<TriangleShape>> splitLargeTriangleShape(const TriangleShape& shape, unsigned maxSize, RTCDevice embreeDevice)
{
    // Split a large shape into smaller shapes with maxSize/2 to maxSize primitives.
    // The Embree BVH builder API used to efficiently partition the shapes primitives into groups.
    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    embreeBuildPrimitives.resize(shape.numPrimitives());
    for (unsigned primID = 0; primID < shape.numPrimitives(); primID++) {
        auto bounds = shape.getPrimitiveBounds(primID);

        RTCBuildPrimitive primitive;
        primitive.lower_x = bounds.min.x;
        primitive.lower_y = bounds.min.y;
        primitive.lower_z = bounds.min.z;
        primitive.upper_x = bounds.max.x;
        primitive.upper_y = bounds.max.y;
        primitive.upper_z = bounds.max.z;
        primitive.geomID = 0;
        primitive.primID = primID;
        embreeBuildPrimitives[primID] = primitive;
    }

    // Build a coarse BVH using the Embree BVH builder API
    RTCBVH bvh = rtcNewBVH(embreeDevice);

    struct UserData {
        const TriangleShape& shape;
        tbb::concurrent_vector<std::shared_ptr<TriangleShape>> subShapes { 0 };
    };
    UserData userData { shape };

    RTCBuildArguments arguments = rtcDefaultBuildArguments();
    arguments.byteSize = sizeof(arguments);
    arguments.buildFlags = RTC_BUILD_FLAG_NONE;
    arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM; // High build quality requires spatial splits
    arguments.maxBranchingFactor = 2;
    arguments.minLeafSize = maxSize; // Stop splitting when number of prims is below minLeafSize
    arguments.maxLeafSize = maxSize; // This is a hard constraint (always split when number of prims is larger than maxLeafSize)
    arguments.bvh = bvh;
    arguments.primitives = embreeBuildPrimitives.data();
    arguments.primitiveCount = embreeBuildPrimitives.size();
    arguments.primitiveArrayCapacity = embreeBuildPrimitives.size();
    arguments.userPtr = &userData;
    arguments.createNode = [](RTCThreadLocalAllocator, unsigned, void*) -> void* {
        return nullptr;
    };
    arguments.setNodeChildren = [](void*, void**, unsigned, void*) {};
    arguments.setNodeBounds = [](void*, const RTCBounds**, unsigned, void*) {};
    arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
        UserData& userData = *reinterpret_cast<UserData*>(userPtr);

        void* pPrimitiveIDsMem = rtcThreadLocalAlloc(alloc, numPrims * sizeof(unsigned), std::alignment_of_v<unsigned>);
        gsl::span<unsigned> primitiveIDs { reinterpret_cast<unsigned*>(pPrimitiveIDsMem), static_cast<gsl::span<unsigned>::index_type>(numPrims) };
        for (size_t i = 0; i < numPrims; i++) {
            primitiveIDs[i] = prims[i].primID;
        }

        userData.subShapes.push_back(std::make_shared<TriangleShape>(userData.shape.subMesh(primitiveIDs)));
        return nullptr;
    };

    rtcBuildBVH(&arguments);
    rtcReleaseBVH(bvh);

    std::vector<std::shared_ptr<TriangleShape>> subShapes;
    std::copy(std::begin(userData.subShapes), std::end(userData.subShapes), std::back_inserter(subShapes));
    return subShapes;
}

std::vector<std::shared_ptr<SceneNode>> createSubScenes(const Scene& scene, unsigned primitivesPerSubScene, RTCDevice embreeDevice)
{
    // Split a large shape into smaller shapes with maxSize/2 to maxSize primitives.
    // The Embree BVH builder API used to efficiently partition the shapes primitives into groups.
    //using Path = eastl::fixed_vector<unsigned, 4>;
    using Path = std::vector<unsigned>;
    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    std::vector<Path> primitivePaths;

    std::function<void(const SceneNode*, Path)> addEmbreePrimitives = [&](const SceneNode* pSceneNode, Path pathToNode) {
        for (const auto& [sceneObjectID, pSceneObject] : enumerate(pSceneNode->objects)) {
            auto path = pathToNode;
            path.push_back(sceneObjectID);
            unsigned primPathID = static_cast<unsigned>(primitivePaths.size());
            primitivePaths.push_back(path);

            if (!pSceneObject->pShape)
                continue;

            Bounds bounds = pSceneObject->pShape->getBounds();

            RTCBuildPrimitive primitive;
            primitive.lower_x = bounds.min.x;
            primitive.lower_y = bounds.min.y;
            primitive.lower_z = bounds.min.z;
            primitive.upper_x = bounds.max.x;
            primitive.upper_y = bounds.max.y;
            primitive.upper_z = bounds.max.z;
            primitive.geomID = 0;
            primitive.primID = primPathID;
            embreeBuildPrimitives.push_back(primitive);
        }

        for (const auto& [childID, childLink] : enumerate(pSceneNode->children)) {
            auto path = pathToNode;
            path.push_back(childID);

            const auto& [pChild, optTransform] = childLink;
            addEmbreePrimitives(pChild.get(), path);
        }
    };
    addEmbreePrimitives(scene.pRoot.get(), Path {});
    ALWAYS_ASSERT(primitivePaths.size() < std::numeric_limits<unsigned>::max());

    // Build a BVH over all scene objects (including instanced ones)
    RTCBVH bvh = rtcNewBVH(embreeDevice);

    struct BVHNode {
        virtual ~BVHNode() {};

        //eastl::fixed_vector<Bounds, 2, false> childBounds;
        std::vector<Bounds> childBounds;
        size_t numPrimitives { 0 };
    };
    struct LeafNode : public BVHNode {
        //eastl::fixed_vector<Path, 2, false> sceneObjectPaths;
        std::vector<Path> sceneObjectPaths;
    };
    struct InnerNode : public BVHNode {
        //eastl::fixed_vector<BVHNode*, 2, false> children;
        std::vector<BVHNode*> children;
    };

    RTCBuildArguments arguments = rtcDefaultBuildArguments();
    arguments.byteSize = sizeof(arguments);
    arguments.buildFlags = RTC_BUILD_FLAG_NONE;
    arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM; // High build quality requires spatial splits
    arguments.maxBranchingFactor = 2;
    arguments.minLeafSize = 1; // Stop splitting when number of prims is below minLeafSize
    arguments.maxLeafSize = 2; // This is a hard constraint (always split when number of prims is larger than maxLeafSize)
    arguments.bvh = bvh;
    arguments.primitives = embreeBuildPrimitives.data();
    arguments.primitiveCount = embreeBuildPrimitives.size();
    arguments.primitiveArrayCapacity = embreeBuildPrimitives.size();
    arguments.userPtr = &primitivePaths;
    arguments.createNode = [](RTCThreadLocalAllocator alloc, unsigned numChildren, void*) -> void* {
        void* pMem = rtcThreadLocalAlloc(alloc, sizeof(InnerNode), std::alignment_of_v<InnerNode>);
        return static_cast<BVHNode*>(new (pMem) InnerNode());
    };
    arguments.setNodeChildren = [](void* pMem, void** ppChildren, unsigned childCount, void*) {
        auto* pNode = static_cast<InnerNode*>(pMem);
        for (unsigned i = 0; i < childCount; i++) {
            pNode->children.push_back(static_cast<BVHNode*>(ppChildren[i]));
        }
    };
    arguments.setNodeBounds = [](void* pMem, const RTCBounds** ppBounds, unsigned childCount, void*) {
        auto* pNode = static_cast<InnerNode*>(pMem);
        for (unsigned i = 0; i < childCount; i++) {
            const auto* pBounds = ppBounds[i];
            pNode->childBounds.push_back(Bounds(*pBounds));
        }
    };
    arguments.createLeaf = [](RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr) -> void* {
        void* pMem = rtcThreadLocalAlloc(alloc, sizeof(InnerNode), std::alignment_of_v<InnerNode>);
        auto* pNode = new (pMem) LeafNode();

        const auto& primitivePaths = *reinterpret_cast<std::vector<Path>*>(userPtr);

        pNode->numPrimitives = static_cast<unsigned>(numPrims);
        for (size_t i = 0; i < numPrims; i++) {
            const auto& prim = prims[i];
            pNode->sceneObjectPaths.emplace_back(std::move(primitivePaths[prim.primID]));
        }
        return static_cast<BVHNode*>(pNode);
    };
    auto* bvhRoot = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    std::function<size_t(BVHNode*)> computePrimCount = [&](BVHNode* pNode) -> size_t {
        if (auto* pInnerNode = dynamic_cast<InnerNode*>(pNode)) {
            pInnerNode->numPrimitives = 0;
            for (auto* pChild : pInnerNode->children)
                pInnerNode->numPrimitives += computePrimCount(pChild);
            return pInnerNode->numPrimitives;
        } else if (auto* pLeafNode = dynamic_cast<LeafNode*>(pNode)) {
            pLeafNode->numPrimitives = 0;

            for (const Path& path : pLeafNode->sceneObjectPaths) {
                const SceneNode* pSceneNode = scene.pRoot.get();
                for (size_t i = 0; i < path.size() - 1; i++)
                    pSceneNode = std::get<std::shared_ptr<SceneNode>>(pSceneNode->children[path[i]]).get();
                const auto& pSceneObject = pSceneNode->objects[path.back()];

                pLeafNode->numPrimitives += pSceneObject->pShape->numPrimitives();
            }

            return pLeafNode->numPrimitives;
        } else {
            throw std::runtime_error("Unknown BVH node type");
        }
    };
    computePrimCount(bvhRoot);

    std::function<std::vector<Path>(const BVHNode*)> flattenSubTree = [&](const BVHNode* pNode) {
        if (const auto* pInnerNode = dynamic_cast<const InnerNode*>(pNode)) {
            std::vector<Path> sceneObjects;
            for (const auto* pChild : pInnerNode->children) {
                auto childSceneObjects = flattenSubTree(pChild);
                sceneObjects.reserve(sceneObjects.size() + childSceneObjects.size());
                std::copy(std::begin(childSceneObjects), std::end(childSceneObjects), std::back_inserter(sceneObjects));
            }
            return sceneObjects;
        } else if (const auto* pLeafNode = dynamic_cast<const LeafNode*>(pNode)) {
            std::vector<Path> sceneObjectPaths;
            std::copy(std::begin(pLeafNode->sceneObjectPaths), std::end(pLeafNode->sceneObjectPaths), std::back_inserter(sceneObjectPaths));
            return sceneObjectPaths;
        } else {
            throw std::runtime_error("Unknown BVH node type");
        }
    };

    std::vector<std::vector<Path>> subSceneObjects;
    std::function<void(const BVHNode*)> computeSubSceneRoots = [&](const BVHNode* pNode) {
        if (pNode->numPrimitives > primitivesPerSubScene) {
            if (const auto* pInnerNode = dynamic_cast<const InnerNode*>(pNode)) {
                for (const auto* pChild : pInnerNode->children)
                    computeSubSceneRoots(pChild);
            } else {
                subSceneObjects.push_back(flattenSubTree(pNode));
            }
        } else {
            subSceneObjects.push_back(flattenSubTree(pNode));
        }
    };
    computeSubSceneRoots(bvhRoot);

    std::vector<std::shared_ptr<SceneNode>> subSceneRoots;
    std::transform(
        std::begin(subSceneObjects),
        std::end(subSceneObjects),
        std::back_inserter(subSceneRoots),
        [&](const std::vector<Path>& paths) -> std::shared_ptr<SceneNode> {
            std::unordered_map<const SceneNode*, std::shared_ptr<SceneNode>> sceneNodeLUT;
            auto getShadowNode = [&](const SceneNode* pOriginalNode) -> std::shared_ptr<SceneNode> {
                if (auto iter = sceneNodeLUT.find(pOriginalNode); iter != std::end(sceneNodeLUT)) {
                    return iter->second;
                } else {
                    auto pShadowNode = std::make_shared<SceneNode>();
                    sceneNodeLUT[pOriginalNode] = pShadowNode;
                    return pShadowNode;
                }
            };

            std::function<std::shared_ptr<SceneNode>(SceneNode*, gsl::span<const unsigned>)> recreatePath = [&](SceneNode* pOriginalSceneNode, gsl::span<const unsigned> path) {
                auto pShadowSceneNode = getShadowNode(pOriginalSceneNode);

                if (path.size() == 1) {
                    // Reached leaf
                    const auto pSceneObject = pOriginalSceneNode->objects[path[0]];
                    pShadowSceneNode->objects.push_back(pSceneObject);
                } else {
                    // Traverse further
                    const auto& [pOriginalChild, optTransform] = pOriginalSceneNode->children[path[0]];
                    pShadowSceneNode->children.push_back({ recreatePath(pOriginalChild.get(), path.subspan(1)), optTransform });
                }

                return pShadowSceneNode;
            };

            std::shared_ptr<SceneNode> pRoot { nullptr };
            for (const auto& path : paths) {
                auto pathSpan = gsl::make_span(path.data(), path.size());
                pRoot = recreatePath(scene.pRoot.get(), pathSpan);
            }
            return pRoot;
        });

    rtcReleaseBVH(bvh);

    return subSceneRoots;
}

RTCScene BatchingAccelerationStructureBuilder::buildRecurse(const SceneNode* pSceneNode, RTCDevice embreeDevice, std::unordered_map<const SceneNode*, RTCScene>& sceneCache)
{
    RTCScene embreeScene = rtcNewScene(embreeDevice);
    for (const auto& pSceneObject : pSceneNode->objects) {
        const Shape* pShape = pSceneObject->pShape.get();
        RTCGeometry embreeGeometry = pShape->createEmbreeGeometry(embreeDevice);
        rtcSetGeometryUserData(embreeGeometry, pSceneObject.get());
        rtcCommitGeometry(embreeGeometry);

        unsigned geometryID = rtcAttachGeometry(embreeScene, embreeGeometry);
        (void)geometryID;
    }

    for (const auto&& [geomID, childLink] : enumerate(pSceneNode->children)) {
        auto&& [pChildNode, optTransform] = childLink;

        RTCScene childScene;
        if (auto iter = sceneCache.find(pChildNode.get()); iter != std::end(sceneCache)) {
            childScene = iter->second;
        } else {
            childScene = buildRecurse(pChildNode.get(), embreeDevice, sceneCache);
            sceneCache[pChildNode.get()] = childScene;
        }

        RTCGeometry embreeInstanceGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
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
        // Offset geomID by 1 so that we never have geometry with ID=0. This way we know that if hit.instID[x] = 0
        // then this means that the value is invalid (since Embree always sets it to 0 when invalid instead of RTC_INVALID_GEOMETRY_ID).
        rtcAttachGeometryByID(embreeScene, embreeInstanceGeometry, geomID + 1);
    }

    rtcCommitScene(embreeScene);
    return embreeScene;
}

/*batching_impl::BatchingPoint createBatchingPoint(const std::shared_ptr<SceneNode> pSubSceneRoot, RTCDevice embreeDevice)
{
    verifyInstanceDepth(pSubSceneRoot.get());

    std::unordered_map<const SceneNode*, RTCScene> sceneCache;
    RTCScene embreeSubScene = buildRecurse(pSubSceneRoot.get(), embreeDevice, sceneCache);

    return batching_impl::BatchingPoint { embreeSubScene, pSubSceneRoot };
}*/

void BatchingAccelerationStructureBuilder::verifyInstanceDepth(const SceneNode* pSceneNode, int depth)
{
    for (const auto& [pChildNode, optTransform] : pSceneNode->children) {
        verifyInstanceDepth(pChildNode.get(), depth + 1);
    }
    ALWAYS_ASSERT(depth <= RTC_MAX_INSTANCE_LEVEL_COUNT);
}

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

}