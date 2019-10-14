#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/samplers/rng/pcg.h"
#include <embree3/rtcore.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace pandora {

class IntersectGeometry {
public:
    // TODO:
    //virtual void serialize() = 0;

    virtual size_t sizeBytes() const = 0;
    virtual RTCGeometry createEmbreeGeometry(RTCDevice embreeDevice) const = 0;

    virtual float primitiveArea(unsigned primitiveID) const = 0;
    virtual Interaction samplePrimitive(unsigned primitiveID, PcgRng& rng) const = 0;
    virtual Interaction samplePrimitive(unsigned primitiveID, const Interaction& ref, PcgRng& rng) const = 0;

    virtual float pdfPrimitive(unsigned primitiveID, const Interaction& ref) const = 0;
    virtual float pdfPrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec3& wi) const = 0;

    virtual void voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const = 0;
};

class ShadingGeometry {
public:
    // TODO:
    //virtual void serialize() = 0;

    virtual SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& hitInfo) const = 0;
};

class Shape {
public:
    virtual ~Shape() = default;

    virtual const IntersectGeometry* getIntersectGeometry() const = 0;
    virtual const ShadingGeometry* getShadingGeometry() const = 0;

    virtual unsigned numPrimitives() const = 0;
    virtual Bounds getBounds() const = 0;
};

}