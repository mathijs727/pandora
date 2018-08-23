#pragma once
#include "pandora/core/material.h"
#include "pandora/core/texture.h"

namespace pandora {

class MirrorMaterial : public Material {
public:
    MirrorMaterial();

    void computeScatteringFunctions(SurfaceInteraction& si, ShadingMemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;
};

}
