#pragma once
#include "pandora/shading/core/bxdf.h"

namespace pandora {

template <typename Fresnel>
class SpecularBxDF : public BxDF {
public:
    SpecularBxDF(const Spectrum& R, const Fresnel& fresnel);

    // Evaluate the BxDF for incoming/outgoing direction pair
    Spectrum f(const Vec3f& wo, const Vec3f& wi) const override;

    // Evaluate the BxDF for the outgoing ray and pick an incoming ray (usefull for reflective surfaces)
    Spectrum sampleF(const Vec3f& wo, Vec3f& wi, const Point2f& sample, float& pdf, BxDFType* sampledType = nullptr) const override;

    // Evaluate the total reflectance (from all incoming directions) at the outgoing direction
    Spectrum rho(const Vec3f& wo, gsl::span<Point2f> samples) const override;
    // Evaluate the total reflectance in all directions from all directions
    Spectrum rho(gsl::span<Point2f> samples) const override;
private:
    Spectrum m_R;
    Fresnel m_fresnel;
};

}