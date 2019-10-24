#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/samplers/rng/pcg.h"
#include <embree3/rtcore.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <stream/evictable.h>

namespace pandora {

class Shape : public stream::Evictable {
public:
    Shape(bool resident);
    virtual ~Shape() = default;

    virtual unsigned numPrimitives() const = 0;
    virtual Bounds getBounds() const = 0;

    virtual RTCGeometry createEmbreeGeometry(RTCDevice embreeDevice) const = 0;

    virtual float primitiveArea(unsigned primitiveID) const = 0;
    virtual Interaction samplePrimitive(unsigned primitiveID, PcgRng& rng) const = 0;
    virtual Interaction samplePrimitive(unsigned primitiveID, const Interaction& ref, PcgRng& rng) const = 0;

    virtual float pdfPrimitive(unsigned primitiveID, const Interaction& ref) const = 0;
    virtual float pdfPrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec3& wi) const = 0;

    virtual SurfaceInteraction fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const = 0;

    virtual void voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const = 0;
};

}