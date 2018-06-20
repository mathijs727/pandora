#pragma once
#include "glm/glm.hpp"
#include "pandora/core/bxdf.h"
#include "pandora/core/pandora.h"
#include "pandora/core/ray.h"

namespace pandora {

struct SceneObject;

struct Interaction {
    Interaction() = default;
    Interaction(const glm::vec3& position, const glm::vec3& wo, const glm::vec3& normal)
        : position(position)
        , wo(wo)
        , normal(normal)
    {
    }
    glm::vec3 position;
    glm::vec3 wo; // Outgoing ray

    glm::vec3 normal;
};

struct SurfaceInteraction : public Interaction {
public:
    const SceneObject* sceneObject = nullptr;
    unsigned primitiveID;

    BSDF* bsdf = nullptr;

    glm::vec2 uv;
    glm::vec3 dpdu, dpdv;
    glm::vec3 dndu, dndv;

    struct Shading {
        glm::vec3 normal;
        glm::vec3 dpdu, dpdv;
        glm::vec3 dndu, dndv;
    } shading;

public:
    SurfaceInteraction(const SceneObject& sceneObject, unsigned primitiveID, const glm::vec3& position, const glm::vec3& geometricNormal, const glm::vec3& uv, const glm::vec3& wo)
        : Interaction(position, geometricNormal, wo)
        , sceneObject(&sceneObject)
        , primitiveID(primitiveID)
        , uv(uv)
    {
    }
    SurfaceInteraction()
        : sceneObject(nullptr)
    {
    }

    void setShadingGeometry(const glm::vec3& dpdus, const glm::vec3& dpdvs, const glm::vec3& dndus, const glm::vec3& dndvs, bool orientationIsAuthoritative);

    void computeScatteringFunctions(const Ray& ray, MemoryArena& arena, TransportMode mode = TransportMode::Radiance, bool allowMultipleLobes = false);

    glm::vec3 lightEmitted(const glm::vec3& w) const;

    Ray spawnRay(const glm::vec3& dir) const;
};

Ray computeRayWithEpsilon(const Interaction& i1, const Interaction& i22);
Ray computeRayWithEpsilon(const Interaction& i1, const glm::vec3& dir);

}