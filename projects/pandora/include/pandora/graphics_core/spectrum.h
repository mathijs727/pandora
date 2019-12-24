#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace pandora {

static const int nCIESamples = 471;
extern const float CIE_X[nCIESamples];
extern const float CIE_Y[nCIESamples];
extern const float CIE_Z[nCIESamples];
extern const float CIE_lambda[nCIESamples];
static const float CIE_Y_integral = 106.856895f;

bool spectrumSamplesSorted(const float *lambda, const float *vals, int n);
void sortSpectrumSamples(float *lambda, float *vals, int n);
float interpolateSpectrumSamples(const float *lambda, const float *vals, int n, float l);

using Spectrum = glm::vec3;
inline bool isBlack(const Spectrum& s)
{
    return glm::dot(s, s) == 0.0f;
}

inline Spectrum XYZToRGB(const float xyz[3])
{
    Spectrum r;
    r[0] = 3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
    r[1] = -0.969256f * xyz[0] + 1.875991f * xyz[1] + 0.041556f * xyz[2];
    r[2] = 0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];
    return r;
}

inline Spectrum fromSampled(const float* lambda, const float* v, int n)
{
    // Sort samples if unordered, use sorted for returned spectrum
    if (!spectrumSamplesSorted(lambda, v, n)) {
        std::vector<float> slambda(&lambda[0], &lambda[n]);
        std::vector<float> sv(&v[0], &v[n]);
        sortSpectrumSamples(&slambda[0], &sv[0], n);
        return fromSampled(&slambda[0], &sv[0], n);
    }
    float xyz[3] = { 0, 0, 0 };
    for (int i = 0; i < nCIESamples; ++i) {
        float val = interpolateSpectrumSamples(lambda, v, n, CIE_lambda[i]);
        xyz[0] += val * CIE_X[i];
        xyz[1] += val * CIE_Y[i];
        xyz[2] += val * CIE_Z[i];
    }
    float scale = float(CIE_lambda[nCIESamples - 1] - CIE_lambda[0]) / float(CIE_Y_integral * nCIESamples);
    xyz[0] *= scale;
    xyz[1] *= scale;
    xyz[2] *= scale;
    return XYZToRGB(xyz);
}

}
