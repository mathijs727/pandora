#pragma once
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/texture.h"

namespace pandora {

class MirrorMaterial : public Material {
public:
    MirrorMaterial();

    void computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena) const final;
};

}
