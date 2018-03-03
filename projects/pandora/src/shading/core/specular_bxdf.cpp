#include "pandora/shading/materials/specular_bxdf.h"

namespace pandora {

template<typename Fresnel>
SpecularBxDF<Fresnel>::SpecularBxDF(const Spectrum& R, const Fresnel& fresnel) :
    BxDF((BxDFType)(BxDF_REFLECTION | BxDF_SPECULAR)),
    m_R(R),
    m_fresnel(fresnel)
{
}

template<typename Fresnel>
Spectrum SpecularBxDF<Fresnel>::f(const Vec3f& wo, const Vec3f& wi) const
{
    return Spectrum(0.0f);
}

template<typename Fresnel>
Spectrum SpecularBxDF<Fresnel>::sampleF(const Vec3f& wo, Vec3f& wi, const Point2f& sample, float& pdf, BxDFType* sampledType) const
{
    wi = Vec3f(-wo.x, -wo.y, wo.z);// normal = (0, 0, 1) in BRDF coordinate system
    pdf = 1.0f;
    return m_fresnel.evaluate(cosTheta(wi)) * m_R / absCosTheta(wi);
}

template<typename Fresnel>
Spectrum SpecularBxDF<Fresnel>::rho(const Vec3f& wo, gsl::span<Point2f> samples) const
{
    return Spectrum();
}

template<typename Fresnel>
Spectrum SpecularBxDF<Fresnel>::rho(gsl::span<Point2f> samples) const
{
    return Spectrum();
}

template class SpecularBxDF<DielectricFresnel>;
template class SpecularBxDF<ConductorFresnel>;

}