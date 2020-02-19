#pragma once
#include "glm/glm.hpp"
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/ray.h"
#include "pandora/utility/memory_arena.h"
#include <optional>

namespace pandora {

struct Interaction {
public:
    Interaction() = default;
    inline Interaction(const glm::vec3& position, const glm::vec3& normal, const glm::vec3& wo)
        : position(position)
        , wo(wo)
        , normal(normal)
    {
    }

    Ray spawnRay(const glm::vec3& d) const;

public:
    glm::vec3 position;
    glm::vec3 wo; // Outgoing ray

    glm::vec3 normal;
};

struct SurfaceInteraction : public Interaction {
public:
    std::optional<glm::mat4> localToWorld;
    const SceneObject* pSceneObject { nullptr };
    BSDF* pBSDF { nullptr };

    glm::vec2 uv { 0 };
    //glm::vec3 dpdu, dpdv;
    //glm::vec3 dndu, dndv;

    struct Shading {
        glm::vec3 normal { 0 };
        glm::vec2 st { 0 };
        //glm::vec3 dpdu, dpdv;
        //glm::vec3 dndu, dndv;

		glm::vec3 batchingPointColor { 1 };
    } shading;

public:
    SurfaceInteraction() = default;
    SurfaceInteraction(const glm::vec3& p, const glm::vec3& normal, const glm::vec2& uv, const glm::vec3& wo);

    void setShadingGeometry(const glm::vec3& shadingNormal, const glm::vec2& textureCoords);

    void computeScatteringFunctions(const Ray& ray, MemoryArena& arena);

    glm::vec3 Le(const glm::vec3& w) const;
};

Ray computeRayWithEpsilon(const Interaction& i1, const Interaction& i22);
Ray computeRayWithEpsilon(const Interaction& i1, const glm::vec3& dir);

}
