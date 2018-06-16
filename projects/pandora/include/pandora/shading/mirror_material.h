#pragma once
#include "pandora/shading/material.h"
#include "pandora/shading/texture.h"

namespace pandora {

class MirrorMaterial : public Material {
public:
    MirrorMaterial();

    EvalResult evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 out) const final;
    SampleResult sampleBSDF(const SurfaceInteraction& surfaceInteraction, gsl::span<glm::vec2> samples) const final;
};

}
