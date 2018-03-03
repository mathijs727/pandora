#include "pandora/shading/core/bxdf.h"
#include "pandora/shading/core/spectrum.h"
#include <algorithm>

// https://github.com/mmp/pbrt-v3/blob/master/src/core/reflection.cpp

namespace pandora {

Spectrum DielectricFresnel::evaluate(float cosIncident) const
{
    float refractionIndexIncident = m_refractionIndexIncident;
    float refractionIndexTransmitted = m_refractionIndexTransmitted;

    cosIncident = std::clamp(cosIncident, -1.0f, 1.0f);

    // Switch if the ray is existing the material (negative cosine)
    if (cosIncident < 0.0f) {
        std::swap(refractionIndexIncident, refractionIndexTransmitted);
        cosIncident = std::abs(cosIncident);
    }

    // Compute cosTransmitted using Snell's law
    float sinIncident = std::sqrt(std::max(0.0f, 1.0f - cosIncident * cosIncident));
    float sinTransmitted = refractionIndexIncident / refractionIndexTransmitted * sinIncident;
    // Handle total internal reflection
    if (sinTransmitted >= 1.0f)
        return 1.0f;
    float cosTransmitted = std::sqrt(std::max(0.0f, 1.0f - sinTransmitted * sinTransmitted));

    float fresnelPolarized = ((refractionIndexTransmitted * cosIncident) - (refractionIndexIncident * cosTransmitted)) /
                             ((refractionIndexTransmitted * cosIncident) + (refractionIndexIncident * cosTransmitted));
    float fresnelParallel = ((refractionIndexIncident * cosIncident) - (refractionIndexTransmitted * cosTransmitted)) /
                            ((refractionIndexIncident * cosIncident) - (refractionIndexTransmitted * cosTransmitted));
    return (fresnelParallel * fresnelParallel + fresnelPolarized * fresnelPolarized) / 2.0f;
}

Spectrum ConductorFresnel::evaluate(float cosIncident) const// k = absorption coefficient
{
    cosIncident = std::clamp(cosIncident, -1.0f, 1.0f);

    Spectrum refractionIndex = m_refractionIndexTransmitted / m_refractionIndexIncident;
    Spectrum refractionIndexK = m_k / m_refractionIndexIncident;

    float cosIncident2 = cosIncident * cosIncident;
    float sinIncident2 = 1.0f - cosIncident2;
    Spectrum refractionIndex2 = refractionIndex * refractionIndex;
    Spectrum refractionIndexK2 = refractionIndexK * refractionIndexK;

    Spectrum t0 = refractionIndex2 - refractionIndexK2 - sinIncident2;
    Spectrum a2plusb2 = sqrt(t0 * t0 + 4 * refractionIndex2 * refractionIndexK2);
    Spectrum t1 = a2plusb2 + cosIncident2;
    Spectrum a = sqrt(0.5f * (a2plusb2 + t0));
    Spectrum t2 = 2.0f * cosIncident * a;
    Spectrum Rs = (t1 - t2) / (t1 + t2);

    Spectrum t3 = cosIncident2 * a2plusb2 + sinIncident2 * sinIncident2;
    Spectrum t4 = t2 * sinIncident2;
    Spectrum Rp = Rs * (t3 - t4) / (t3 + t4);

    return 0.5f * (Rp + Rs);
}

}
