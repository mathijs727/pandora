#pragma once
#include "glm/glm.hpp"
#include "pandora/core/bxdf.h"
#include "pandora/core/pandora.h"
#include "pandora/core/ray.h"

namespace pandora {

struct Interaction {
public:
    Interaction() = default;
    inline Interaction(const glm::vec3& position, const glm::vec3& wo, const glm::vec3& normal)
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
    const SceneObject* sceneObject = nullptr;

    unsigned primitiveID;
	const TriangleMesh* shape = nullptr;

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
	SurfaceInteraction()
		: sceneObject(nullptr)
	{
	}
	SurfaceInteraction(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wo, const glm::vec3& dpdu, const glm::vec3& dpdv, const glm::vec3& dndu, const glm::vec3& dndv, const TriangleMesh* shape, unsigned primitiveID);

    void setShadingGeometry(const glm::vec3& dpdus, const glm::vec3& dpdvs, const glm::vec3& dndus, const glm::vec3& dndvs, bool orientationIsAuthoritative);

    void computeScatteringFunctions(const Ray& ray, MemoryArena& arena, TransportMode mode = TransportMode::Radiance, bool allowMultipleLobes = false);

    glm::vec3 Le(const glm::vec3& w) const;
};

inline SurfaceInteraction::SurfaceInteraction(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wo, const glm::vec3& dpdu, const glm::vec3& dpdv, const glm::vec3& dndu, const glm::vec3& dndv, const TriangleMesh* shape, unsigned primitiveID)
	: Interaction(p, glm::normalize(glm::cross(dpdu, dpdv)), wo)
    , sceneObject(nullptr)
	, primitiveID(primitiveID)
    , shape(shape)
    , bsdf(nullptr)
    , uv(uv)
    , dpdu(dpdu)
    , dpdv(dpdv)
    , dndu(dndu)
    , dndv(dndv)
{
	// Initialize shading geometry from true geometry
	shading.normal = normal;
	shading.dpdu = dpdu;
	shading.dpdv = dpdv;
	shading.dndu = dndu;
	shading.dndv = dndv;

	// TODO: adjust normal based on orientation and handedness
}

Ray computeRayWithEpsilon(const Interaction& i1, const Interaction& i22);
Ray computeRayWithEpsilon(const Interaction& i1, const glm::vec3& dir);

}
