#pragma once
#include "pandora/core/globals.h"
#include "pandora/shading/core/bxdf.h"

namespace pandora {

template <typename BxDFType>
class ScaledBxDF : public BxDF {
public:
	ScaledBxDF(const BxDFType& bxdf, const Spectrum& scale);

	// Evaluate the BxDF for incoming/outgoing direction pair
	Spectrum f(const Vec3f& wo, const Vec3f& wi) const override;

	// Evaluate the BxDF for the outgoing ray and pick an incoming ray (usefull for reflective surfaces)
	Spectrum sampleF(const Vec3f& wo, Vec3f& wi, const Point2f& sample, float& pdf, BxDFType* sampledType = nullptr) const override;

	// Evaluate the total reflectance (from all incoming directions) at the outgoing direction
	Spectrum rho(const Vec3f& wo, gsl::span<Point2f> samples) const override;
	// Evaluate the total reflectance in all directions from all directions
	Spectrum rho(gsl::span<Point2f> samples) const override;
private:
	BxDFType m_bxdf;
	Spectrum m_scale;
};

template<typename BxDFType>
inline ScaledBxDF<BxDFType>::ScaledBxDF(const BxDFType& bxdf, const Spectrum& scale) :
	BxDF(bxdf.getType())
	m_bxdf(bxdf),
	m_scale(scale)
{
}

template<typename BxDFType>
inline Spectrum ScaledBxDF<BxDFType>::f(const Vec3f& wo, const Vec3f& wi) const
{
	return scale * m_bxdf.f(wo, wi);
}

template<typename BxDFType>
inline Spectrum ScaledBxDF<BxDFType>::sampleF(const Vec3f& wo, Vec3f& wi, const Point2f& sample, float& pdf, BxDFType* sampledType) const
{
	return scale * m_bxdf.sampleF(wo, wi, sample, pdf, sampledType);
}

template<typename BxDFType>
inline Spectrum ScaledBxDF<BxDFType>::rho(const Vec3f& wo, gsl::span<Point2f> samples) const
{
	return scale * m_bxdf.rho(wo, samples);
}

template<typename BxDFType>
inline Spectrum ScaledBxDF<BxDFType>::rho(gsl::span<Point2f> samples) const
{
	return scale * m_bxdf.rho(samples);
}

}