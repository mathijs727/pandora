#pragma once
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/traversal/sub_scene.h"
#include <embree3/rtcore.h>
#include <memory>
#include <stream/cache/cache.h>
#include <stream/cache/cached_ptr.h>
#include <stream/cache/lru_cache_ts.h>
#include <vector>

namespace pandora::detail {

std::vector<pandora::SubScene> createSubScenes(const pandora::Scene& scene, unsigned primitivesPerSubScene, RTCDevice embreeDevice);
std::vector<pandora::Shape*> getSubSceneShapes(const SubScene& subScene);
std::vector<tasking::CachedPtr<Shape>> makeSubSceneResident(const pandora::SubScene& subScene, tasking::LRUCacheTS& geometryCache);
pandora::SparseVoxelDAG createSVDAGfromSubScene(const pandora::SubScene& subScene, int resolution);
void splitLargeSceneObjects(pandora::SceneNode* pSceneNode, tasking::LRUCacheTS& oldCache, tasking::CacheBuilder& newCacheBuilder, RTCDevice embreeDevice, unsigned maxSize);

std::vector<std::vector<const SceneObject*>> createSceneObjectGroups(const Scene& scene, unsigned primitivesPerSubScene, RTCDevice embreeDevice);
std::vector<Shape*> getInstancedShapes(const Scene& scene);
Bounds computeSceneObjectGroupBounds(gsl::span<const SceneObject* const> sceneObjects);
SparseVoxelDAG createSVDAGfromSceneObjects(gsl::span<const SceneObject* const> sceneObjects, int resolution);

RTCScene buildInstanceEmbreeScene(const Scene& scene, RTCDevice device);
bool intersectInstanceEmbreeScene(const RTCScene scene, Ray& ray, SurfaceInteraction& si);
bool intersectAnyInstanceEmbreeScene(const RTCScene scene, Ray& ray);

}