#include "pandora/core/material.h"
#include "pandora/core/interaction.h"

namespace pandora {
BSDF::BSDF(const SurfaceInteraction& si, float eta)
    : m_eta(eta)
    , m_ns(si.shading.normal)
    , m_ng(si.normal)
    , m_ss(glm::normalize(si.shading.dpdu))
    , m_ts(glm::cross(m_ns, m_ss))
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

    Spectrum f(0.0f);
    for (const BxDF* bxdf : m_bxdfs) {
        if (bxdf->matchesFlags(flags) && ((reflect && bxdf->getType() & BSDF_REFLECTION)) || (!reflect && bxdf->getType() & BSDF_TRANSMISSION))
            f += bxdf->f(wo, wi);
    }
    return f;
}

Spectrum BSDF::rho(const glm::vec3& wo, gsl::span<const glm::vec2> samples, BxDFType flags) const
{
    Spectrum result(0.0f);
    for (const BxDF* bxdf : m_bxdfs)
        if (bxdf->matchesFlags(flags))
            result += bxdf->rho(wo, samples);
    return result;
}

Spectrum BSDF::rho(gsl::span<const glm::vec2> samples1, gsl::span<const glm::vec2> samples2, BxDFType flags) const
{
    Spectrum result(0.0f);
    for (const BxDF* bxdf : m_bxdfs)
        if (bxdf->matchesFlags(flags))
            result += bxdf->rho(samples1, samples2);
    return result; 
}

}
