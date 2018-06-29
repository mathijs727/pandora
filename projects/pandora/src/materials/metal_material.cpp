#include "pandora/materials/metal_material.h"
#include "glm/glm.hpp"
#include "pandora/core/interaction.h"
#include "pandora/textures/constant_texture.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/fresnel.h"
#include "reflection/lambert_bxdf.h"
#include "reflection/microfacet.h"
#include "reflection/microfacet_bxdf.h"
#include "reflection/oren_nayer_bxdf.h"
#include "shading.h"
#include <functional>

// https://github.com/mmp/pbrt-v3/blob/master/src/materials/metal.cpp

namespace pandora {

MetalMaterial::MetalMaterial(
    const std::shared_ptr<Texture<Spectrum>>& eta,
    const std::shared_ptr<Texture<Spectrum>>& k,
    const std::shared_ptr<Texture<float>>& urough,
    const std::shared_ptr<Texture<float>>& vrough,
    bool remapRoughness) :
	m_eta(eta),
	m_k(k),
	m_roughness(nullptr),
	m_uRoughness(urough),
	m_vRoughness(vrough),
	m_remapRoughness(remapRoughness)
{
}

MetalMaterial::MetalMaterial(
	const std::shared_ptr<Texture<Spectrum>>& eta,
	const std::shared_ptr<Texture<Spectrum>>& k,
	const std::shared_ptr<Texture<float>>& rough,
	bool remapRoughness) :
	m_eta(eta),
	m_k(k),
	m_roughness(rough),
	m_uRoughness(nullptr),
	m_vRoughness(nullptr),
	m_remapRoughness(remapRoughness)
{
}


// PBRTv3 page 581
void MetalMaterial::computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const
{
    // TODO: perform bump mapping (normal mapping)

    si.bsdf = arena.allocate<BSDF>(si);

	float uRough = m_uRoughness ? m_uRoughness->evaluate(si) : m_roughness->evaluate(si);
	float vRough = m_vRoughness ? m_vRoughness->evaluate(si) : m_roughness->evaluate(si);
	if (m_remapRoughness) {
		uRough = TrowbridgeReitzDistribution::roughnessToAlpha(uRough);
		vRough = TrowbridgeReitzDistribution::roughnessToAlpha(vRough);
	}
	Fresnel* frMf = arena.allocate<FresnelConductor>(Spectrum(1.0f), m_eta->evaluate(si), m_k->evaluate(si));

	MicrofacetDistribution* distrib = arena.allocate<TrowbridgeReitzDistribution>(uRough, vRough);
	si.bsdf->add(arena.allocate<MicrofacetReflection>(Spectrum(1.0f), std::ref(*distrib), std::ref(*frMf)));
}

// https://github.com/mmp/pbrt-v3/blob/master/src/materials/metal.cpp
const int CopperSamples = 56;
const float CopperWavelengths[CopperSamples] = {
	298.7570554f, 302.4004341f, 306.1337728f, 309.960445f,  313.8839949f,
	317.9081487f, 322.036826f,  326.2741526f, 330.6244747f, 335.092373f,
	339.6826795f, 344.4004944f, 349.2512056f, 354.2405086f, 359.374429f,
	364.6593471f, 370.1020239f, 375.7096303f, 381.4897785f, 387.4505563f,
	393.6005651f, 399.9489613f, 406.5055016f, 413.2805933f, 420.2853492f,
	427.5316483f, 435.0322035f, 442.8006357f, 450.8515564f, 459.2006593f,
	467.8648226f, 476.8622231f, 486.2124627f, 495.936712f,  506.0578694f,
	516.6007417f, 527.5922468f, 539.0616435f, 551.0407911f, 563.5644455f,
	576.6705953f, 590.4008476f, 604.8008683f, 619.92089f,   635.8162974f,
	652.5483053f, 670.1847459f, 688.8009889f, 708.4810171f, 729.3186941f,
	751.4192606f, 774.9011125f, 799.8979226f, 826.5611867f, 855.0632966f,
	885.6012714f };

const float CopperN[CopperSamples] = {
	1.400313f, 1.38f,  1.358438f, 1.34f,  1.329063f, 1.325f, 1.3325f,   1.34f,
	1.334375f, 1.325f, 1.317812f, 1.31f,  1.300313f, 1.29f,  1.281563f, 1.27f,
	1.249062f, 1.225f, 1.2f,      1.18f,  1.174375f, 1.175f, 1.1775f,   1.18f,
	1.178125f, 1.175f, 1.172812f, 1.17f,  1.165312f, 1.16f,  1.155312f, 1.15f,
	1.142812f, 1.135f, 1.131562f, 1.12f,  1.092437f, 1.04f,  0.950375f, 0.826f,
	0.645875f, 0.468f, 0.35125f,  0.272f, 0.230813f, 0.214f, 0.20925f,  0.213f,
	0.21625f,  0.223f, 0.2365f,   0.25f,  0.254188f, 0.26f,  0.28f,     0.3f };

const float CopperK[CopperSamples] = {
	1.662125f, 1.687f, 1.703313f, 1.72f,  1.744563f, 1.77f,  1.791625f, 1.81f,
	1.822125f, 1.834f, 1.85175f,  1.872f, 1.89425f,  1.916f, 1.931688f, 1.95f,
	1.972438f, 2.015f, 2.121562f, 2.21f,  2.177188f, 2.13f,  2.160063f, 2.21f,
	2.249938f, 2.289f, 2.326f,    2.362f, 2.397625f, 2.433f, 2.469187f, 2.504f,
	2.535875f, 2.564f, 2.589625f, 2.605f, 2.595562f, 2.583f, 2.5765f,   2.599f,
	2.678062f, 2.809f, 3.01075f,  3.24f,  3.458187f, 3.67f,  3.863125f, 4.05f,
	4.239563f, 4.43f,  4.619563f, 4.817f, 5.034125f, 5.26f,  5.485625f, 5.717f };

std::shared_ptr<MetalMaterial> MetalMaterial::createCopper(const std::shared_ptr<Texture<float>>& roughness, bool remapRoughness)
{
	static Spectrum copperN = fromSampled(CopperWavelengths, CopperN, CopperSamples);
	auto eta = std::make_shared<ConstantTexture<Spectrum>>(copperN);

	static Spectrum copperK = fromSampled(CopperWavelengths, CopperK, CopperSamples);
	auto k = std::make_shared<ConstantTexture<Spectrum>>(copperK);

	return std::make_shared<MetalMaterial>(eta, k, roughness, remapRoughness);
}

std::shared_ptr<MetalMaterial> MetalMaterial::createCopper(const std::shared_ptr<Texture<float>>& uRoughness, const std::shared_ptr<Texture<float>>& vRoughness, bool remapRoughness)
{
	static Spectrum copperN = fromSampled(CopperWavelengths, CopperN, CopperSamples);
	auto eta = std::make_shared<ConstantTexture<Spectrum>>(copperN);

	static Spectrum copperK = fromSampled(CopperWavelengths, CopperK, CopperSamples);
	auto k = std::make_shared<ConstantTexture<Spectrum>>(copperK);

	return std::make_shared<MetalMaterial>(eta, k, uRoughness, vRoughness, remapRoughness);
}

}
