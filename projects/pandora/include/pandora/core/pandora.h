#pragma once
#include "glm/glm.hpp"

namespace pandora {

using Spectrum = glm::vec3;
inline bool isBlack(const Spectrum& s)
{
    return glm::dot(s, s) == 0.0f;
}


// Forward declares
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

template <class T>
class Texture;
template <class T>
class ConstantTexture;
template <class T>
class ImageTexture;

class SceneObject;
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
