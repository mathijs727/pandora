#pragma once
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/traversal/sub_scene.h"
#include <embree3/rtcore.h>
#include <memory>
#include <stream/cache/cache.h>
#include <stream/cache/cached_ptr.h>
#include <vector>

namespace pandora::detail {

std::vector<pandora::SubScene> createSubScenes(const pandora::Scene& scene, unsigned primitivesPerSubScene, RTCDevice embreeDevice);
std::vector<pandora::Shape*> getSubSceneShapes(const SubScene& subScene);
std::vector<tasking::CachedPtr<Shape>> makeSubSceneResident(const pandora::SubScene& subScene, tasking::LRUCache& geometryCache);
pandora::SparseVoxelDAG createSVDAGfromSubScene(const pandora::SubScene& subScene, int resolution);
void splitLargeSceneObjects(pandora::SceneNode* pSceneNode, tasking::LRUCache& oldCache, tasking::CacheBuilder& newCacheBuilder, RTCDevice embreeDevice, unsigned maxSize);

}