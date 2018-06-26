#pragma once
#include "pandora/core/pandora.h"

namespace pandora {

// Fresnel dielectric
float frDielectric(float cosThetaI, float etaI, float etaT);

// Fresnel conductor
Spectrum frConductor(float cosThetaI, const Spectrum& etaI, const Spectrum& etaT, const Spectrum& k);

class Fresnel {
public:
    virtual Spectrum evaluate(float cosThetaI) const = 0;
};

class FresnelDielectric : public Fresnel {
public:
    FresnelDielectric(float etaI, float etaT);

    Spectrum evaluate(float cosThetaI) const final;

private:
    float m_etaI, m_etaT;
};

class FresnelConductor : public Fresnel {
public:
    FresnelConductor(const Spectrum& etaI, const Spectrum& etaT, const Spectrum& k);

    Spectrum evaluate(float cosThetaI) const final;

private:
    Spectrum m_etaI, m_etaT, m_k;
};

// Total reflection for all incoming directions
class FresnelNoOp : public Fresnel {
public:
    Spectrum evaluate(float cosThetaI) const final;
};

}
