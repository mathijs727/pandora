#include "pandora/materials/mirror_material.h"
#include "glm/glm.hpp"
#include "pandora/graphics_core/interaction.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/specular_bxdf.h"
#include "shading.h"

namespace pandora {

MirrorMaterial::MirrorMaterial()
{
}

void MirrorMaterial::computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena) const
{
    // TODO: perform bump mapping (normal mapping)

    // Evaluate textures and allocate BRDF
    si.pBSDF = arena.allocate<BSDF>(si);

    Fresnel* fresnel = arena.allocate<FresnelDielectric>(1.0f, 1.5f);
    si.pBSDF->add(arena.allocate<SpecularReflection>(Spectrum(1.0f), std::ref(*fresnel)));
    //si.bsdf->add(arena.allocate<SpecularTransmission>(Spectrum(1.0f), 1.333f, 1.333f, TransportMode::Radiance));
}

}
