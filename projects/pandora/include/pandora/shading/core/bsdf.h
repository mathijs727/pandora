#pragma once
#include "pandora/math/vec3.h"
#include "pandora/shading/core/surface_interaction.h"
#include "pandora/shading/core/bxdf.h"
#include "pandora/core/template_magic.h"

#include <gsl/span>
#include <tuple>

namespace pandora {

class BSDF {
public:
    virtual Spectrum f(const Vec3f& woW, const Vec3f& wiW, BxDFType flags) const = 0;
};

template <class... Ts >
class BSDFImpl : public BSDF {
public:
    BSDFImpl(const SurfaceInteraction& si, float relativeIndexOfRefraction = 1.0f, Ts... ts) :
        m_relativeIndexOfRefraction(relativeIndexOfRefraction),
        m_shadingNormal(si.shading.n),
        m_geometryNormal(si.n),
        m_shadingTangentS(si.shading.dpdu.normalized()),// ss
        m_shadingTangentT(cross(m_shadingNormal, m_shadingTangentS)), // ts
        m_BxDFs(std::forward<Ts>(ts)...)
    {
    }

    Spectrum f(const Vec3f& woW, const Vec3f& wiW, BxDFType flags) const override
    {
        Vec3f wi = worldToLocal(wiW);
        Vec3f wo = worldToLocal(woW);
        bool reflect = dot(wiW, m_geometryNormal) * dot(woW, m_geometryNormal) > 0.0f;
        return accumulate_tuple(m_BxDFs, [&](const auto& _BxDF) {
            if (_BxDF.matchesFlags(flags) &&
                ((reflect && (_BxDF.getType() & BxDF_REFLECTION)) ||
                (!reflect && (_BxDF.getType() & BxDF_TRANSMISSION)))) {
                return _BxDF.f(wo, wi);
            }
            else {
                return Spectrum(0.0f);
            }
        });
    }

    /*Spectrum rho(gsl::span<const Point2f> samples1, gsl::span<const Point2f> samples2, BxDFType flags = BXDF_ALL) const
    {
        return Spectrum(0.0f);
    }

    Spectrum rho(const Vec3f& wo, gsl::span<const Point2f> samples, BxDFType flags = BXDF_ALL) const
    {
        return Spectrum(0.0f);
    }*/
private:
    Vec3f worldToLocal(const Vec3f& v) const
    {
        return Vec3f(dot(v, m_shadingTangentS), dot(v, m_shadingTangentT), dot(v, m_shadingNormal));
    }

    Vec3f localToWorld(const Vec3f& v) const
    {
        return v.x * m_shadingTangentS + v.y * m_shadingTangentT + v.z * m_shadingNormal;
    }
private:
    const float m_relativeIndexOfRefraction;// Relative index of refraction over the boundary (only for differential geometry)
    const Normal3f m_shadingNormal, m_geometryNormal;
    const Vec3f m_shadingTangentS, m_shadingTangentT;// Axis normal to each other and the shading normal. Together they create an orthonormal basis.

    std::tuple<Ts...> m_BxDFs;
};



}