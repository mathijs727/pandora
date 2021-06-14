#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/interaction.h"
#include <iostream>

static glm::vec3 arbitraryNonParallelVec(const glm::vec3& ref)
{
    if (ref.x > 0.5f)
        return glm::vec3(0, 1, 0);
    else
        return glm::vec3(1, 0, 0);
}

namespace pandora {
BSDF::BSDF(const SurfaceInteraction& si) //, float eta)
    : m_ns(si.shading.normal)
    , m_ng(si.normal)
    , m_ss(glm::normalize(glm::cross(m_ns, arbitraryNonParallelVec(m_ns))))
    , m_ts(glm::normalize(glm::cross(m_ns, m_ss)))
{
}

void BSDF::add(const BxDF* b)
{
    m_bxdfs.push_back(b);
}

int BSDF::numComponents(BxDFType flags) const
{
    int num = 0;
    for (const BxDF* bxdf : m_bxdfs)
        if (bxdf->matchesFlags(flags))
            num++;
    return num;
}

glm::vec3 BSDF::worldToLocal(const glm::vec3& v) const
{
    return glm::vec3(glm::dot(v, m_ss), glm::dot(v, m_ts), glm::dot(v, m_ns));
}

glm::vec3 BSDF::localToWorld(const glm::vec3& v) const
{
    return glm::vec3(
        m_ss.x * v.x + m_ts.x * v.y + m_ns.x * v.z,
        m_ss.y * v.x + m_ts.y * v.y + m_ns.y * v.z,
        m_ss.z * v.x + m_ts.z * v.y + m_ns.z * v.z);
}

Spectrum BSDF::f(const glm::vec3& woW, const glm::vec3& wiW, BxDFType flags) const
{
    glm::vec3 wi = worldToLocal(wiW), wo = worldToLocal(woW);
    bool reflect = glm::dot(wiW, m_ng) * glm::dot(woW, m_ng) > 0.0f;

    glm::vec3 localShadingNormal = worldToLocal(m_ns);
    glm::vec3 localGeometricNormal = worldToLocal(m_ng);
    (void)localShadingNormal;
    (void)localGeometricNormal;

    Spectrum f(0.0f);
    for (const BxDF* bxdf : m_bxdfs) {
        if (bxdf->matchesFlags(flags) && (((reflect && bxdf->getType() & BSDF_REFLECTION)) || (!reflect && bxdf->getType() & BSDF_TRANSMISSION)))
            f += bxdf->f(wo, wi);
    }

    return f;
}

std::optional<BSDF::Sample> BSDF::sampleF(const glm::vec3& woWorld, const glm::vec2& u, BxDFType type)
{
    // Choose which BxDF to sample
    int matchingComps = numComponents(type);
    if (matchingComps == 0) {
        return {};
    }
    int comp = std::min((int)std::floor(u[0] * matchingComps), matchingComps - 1);

    // Get BxDF pointer for chosen component
    const BxDF* bxdf = nullptr;
    int count = comp;
    for (auto tmpBxdf : m_bxdfs) {
        if (tmpBxdf->matchesFlags(type) && count-- == 0) {
            bxdf = tmpBxdf;
            break;
        }
    }

    // Remap BxDF sample u to [0, 1]^2   (PBRTv3 page 833)
    glm::vec2 uRemapped(u[0] * matchingComps - comp, u[1]);

    // Sample chosen BxDF
    glm::vec3 wo = worldToLocal(woWorld);
    auto sample = bxdf->sampleF(wo, uRemapped, bxdf->getType());
    if (sample.pdf == 0.0f)
        return {};

    glm::vec3 wi = sample.wi;
    glm::vec3 wiWorld = localToWorld(wi);
    float pdf = sample.pdf;
    BxDFType sampledType = bxdf->getType();
    Spectrum f = sample.f;

    // Compute overall PDF with all matching BxDFs
    if (!(bxdf->getType() & BSDF_SPECULAR) && matchingComps > 1)
        for (auto tmpBxdf : m_bxdfs)
            if (tmpBxdf != bxdf && tmpBxdf->matchesFlags(type))
                pdf += tmpBxdf->pdf(wo, wi);
    if (matchingComps > 1)
        pdf /= matchingComps;

    // Compute value of BSDF for sampled direction
    if (!(bxdf->getType() & BSDF_SPECULAR) && matchingComps > 1) {
        bool reflect = glm::dot(wiWorld, m_ng) * glm::dot(woWorld, m_ng) > 0.0f;
        f = Spectrum(0.0f);
        for (auto tmpBxdf : m_bxdfs)
            if (tmpBxdf->matchesFlags(type) && ((reflect && (tmpBxdf->getType() & BSDF_REFLECTION)) || (!reflect && (tmpBxdf->getType() & BSDF_TRANSMISSION)))) {
                f += tmpBxdf->f(wo, wi);
            }
    }
    return Sample {
        wiWorld,
        pdf,
        f,
        sampledType
    };
}

Spectrum BSDF::rho(const glm::vec3& wo, std::span<const glm::vec2> samples, BxDFType flags) const
{
    Spectrum result(0.0f);
    for (const BxDF* bxdf : m_bxdfs)
        if (bxdf->matchesFlags(flags))
            result += bxdf->rho(wo, samples);
    return result;
}

Spectrum BSDF::rho(std::span<const glm::vec2> samples1, std::span<const glm::vec2> samples2, BxDFType flags) const
{
    Spectrum result(0.0f);
    for (const BxDF* bxdf : m_bxdfs)
        if (bxdf->matchesFlags(flags))
            result += bxdf->rho(samples1, samples2);
    return result;
}

float BSDF::pdf(const glm::vec3& woWorld, const glm::vec3& wiWorld, BxDFType flags) const
{
    if (m_bxdfs.empty())
        return 0.0f;

    glm::vec3 wo = worldToLocal(woWorld);
    glm::vec3 wi = worldToLocal(wiWorld);
    if (wo.z == 0.0f)
        return 0.0f;

    float pdf = 0.0f;
    int matchingComps = 0;
    for (auto bxdf : m_bxdfs) {
        if (bxdf->matchesFlags(flags)) {
            matchingComps++;
            pdf += bxdf->pdf(wo, wi);
        }
    }
    float v = matchingComps > 0 ? pdf / matchingComps : 0.0f;
    return v;
}

}
