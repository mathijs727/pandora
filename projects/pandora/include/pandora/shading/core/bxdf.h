#pragma once
#include "pandora/core/globals.h"
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"
#include "pandora/shading/core/spectrum.h"
#include <algorithm>
#include <gsl/gsl>

// https://github.com/mmp/pbrt-v3/blob/master/src/core/reflection.cpp

namespace pandora {

enum BxDFType {
	BxDF_REFLECTION   = 1 << 0,
	BxDF_TRANSMISSION = 1 << 1,
	BxDF_DIFFUSE      = 1 << 2,
	BxDF_GLOSSY       = 1 << 3,
	BxDF_SPECULAR     = 1 << 4,
	BxDF_ALL = BxDF_REFLECTION | BxDF_TRANSMISSION | BxDF_DIFFUSE | BxDF_GLOSSY | BxDF_SPECULAR
};

class BxDF {
public:
	BxDF(BxDFType type) : m_type(type) { }

	// Evaluate the BxDF for incoming/outgoing direction pair
	virtual Spectrum f(const Vec3f& wo, const Vec3f& wi) const = 0;

	// Evaluate the BxDF for the outgoing ray and pick an incoming ray (usefull for reflective surfaces)
	virtual Spectrum sampleF(const Vec3f& wo, Vec3f& wi, const Point2f& sample, float& pdf, BxDFType* sampledType = nullptr) const = 0;

	// Evaluate the total reflectance (from all incoming directions) at the outgoing direction
	virtual Spectrum rho(const Vec3f& wo, gsl::span<Point2f> samples) const = 0;
	// Evaluate the total reflectance in all directions from all directions
	virtual Spectrum rho(gsl::span<Point2f> samples) const = 0;

    bool matchesFlags(BxDFType t) const { return (m_type & t) == m_type; }
	BxDFType getType() const { return m_type; }
private:
	const BxDFType m_type;
};

 
class DielectricFresnel {
public:
    DielectricFresnel(float refractionIndexIncident, float refractionIndexTransmitted) :
        m_refractionIndexIncident(refractionIndexIncident),
        m_refractionIndexTransmitted(refractionIndexTransmitted) { }

    Spectrum evaluate(float cosAngleIncident) const;
private:
    float m_refractionIndexIncident;
    float m_refractionIndexTransmitted;
};

class ConductorFresnel {
public:
    ConductorFresnel(const Spectrum& refractionIndexIncident, const Spectrum& refractionIndexTransmitted, const Spectrum& absorptionCoefficient) :
        m_refractionIndexIncident(refractionIndexIncident),
        m_refractionIndexTransmitted(refractionIndexTransmitted),
        m_k(absorptionCoefficient) { }

    Spectrum evaluate(float cosIncident) const;
private:
    Spectrum m_refractionIndexIncident;
    Spectrum m_refractionIndexTransmitted;
    Spectrum m_k;// Absorption Coefficient
};

inline float cosTheta(const Vec3f& w) { return w.z; }
inline float cos2theta(const Vec3f& w) { return w.z * w.z; }
inline float absCosTheta(const Vec3f& w) { return std::abs(w.z); }

inline float sin2theta(const Vec3f& w) { return std::max(0.0f, 1.0f - cos2theta(w)); }
inline float sinTheta(const Vec3f& w) { return std::sqrt(sin2theta(w)); }

inline float tanTheta(const Vec3f& w) { return sinTheta(w) / cosTheta(w); }
inline float tan2theta(const Vec3f& w) { return sin2theta(w) / cos2theta(w); }

inline float cosPhi(const Vec3f& w)
{
	float _sinTheta = sinTheta(w);
	return (_sinTheta == 0) ? 1 : std::clamp(w.x / _sinTheta, -1.0f, 1.0f);
}
inline float sinPhi(const Vec3f w)
{
	float _sinTheta = sinTheta(w);
	return (_sinTheta == 0) ? 1 : std::clamp(w.y / _sinTheta, -1.0f, 1.0f);
}

inline float cos2phi(const Vec3f& w) { return cosPhi(w) * cosPhi(w); }
inline float sin2phi(const Vec3f& w) { return sinPhi(w) * sinPhi(w); }

inline float cosDPhi(const Vec3f& wa, const Vec3f& wb)
{
	return std::clamp((wa.x * wb.x + wa.y * wb.y) / std::sqrt((wa.x * wa.x + wa.y * wa.y) * (wb.x * wb.x + wb.y * wb.y)), -1.0f, 1.0f);
}

inline Vec3f reflect(const Vec3f& wo, const Vec3f& n)
{
    return -wo + 2.0f * dot(wo, n) * n;
}

}