#pragma once
#include <glm/glm.hpp>
#include "pandora/graphics_core/spectrum.h"
#include "stream/static_data_cache.h"

namespace pandora {

// Forward declares
struct Interaction;
struct SurfaceInteraction;
struct LightSample;
class Transform;

//class InMemoryResource;
//template <typename T>
//class FifoCache;
//template <typename... T>
//class LRUCache;

class Light;
class InfiniteLight;
class DistantLight;
class EnvironmentLight;
class AreaLight;

class BxDF;
class BSDF;
class Material;
class LambertMaterial;
class MirrorMaterial;

class Sampler;
class UniformSampler;

template <class T>
class Texture;
template <class T>
class ConstantTexture;
template <class T>
class ImageTexture;

class SceneObjectMaterial;
class SceneObjectGeometry;
class InCoreSceneObject;
class OOCSceneObject;
class Scene;
class TriangleMesh;
struct Bounds;
struct Ray;
struct RayHit;

class MemoryArena;
class MemoryArenaTS;

class PathIntegrator;
class PerspectiveCamera;
class Sensor;

class VoxelGrid;
class SparseVoxelOctree;

/*template <typename, size_t>
class OOCBatchingAccelerationStructure;
template <typename T, size_t>
class InCoreBatchingAccelerationStructure;
template <typename T>
class InCoreAccelerationStructure;*/

}
