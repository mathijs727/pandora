#pragma once
#include "glm/glm.hpp"

namespace pandora {

using Spectrum = glm::vec3;

struct Interaction;
struct SurfaceInteraction;
struct LightSample;

class Light;
class AreaLight;

class BxDF;
class BSDF;
class Material;
class LambertMaterial;
class MirrorMaterial;

class Sampler;
class UniformSampler;

class Texture;
class ConstantTexture;
class ImageTexture;

class Scene;
class TriangleMesh;
struct Bounds;
struct Ray;

class MemoryArena;
class MemoryArenaTS;

class PathIntegrator;
class PerspectiveCamera;
class Sensor;
}
