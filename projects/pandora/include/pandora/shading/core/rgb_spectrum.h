#pragma once
#include "spectrum.h"

// Copied from PBRTv3
// https://github.com/mmp/pbrt-v3/blob/master/src/core/spectrum.h

namespace pandora {

class RGBSpectrum : public CoefficientSpectrum<3> {
public:
	RGBSpectrum(float v = 0.0f) : CoefficientSpectrum<3>(v) {}
	RGBSpectrum(const CoefficientSpectrum<3>& v) : CoefficientSpectrum<3>(v) { }

	static RGBSpectrum fromRGB(const float rgb[3])
	{
		RGBSpectrum spectrum;
		spectrum.m_coefficients[0] = rgb[0];
		spectrum.m_coefficients[1] = rgb[1];
		spectrum.m_coefficients[2] = rgb[2];
		return spectrum;
	}

	void toRGB(float* rgb) const
	{
		rgb[0] = m_coefficients[0];
		rgb[1] = m_coefficients[1];
		rgb[2] = m_coefficients[2];
	}

    friend RGBSpectrum sqrt(const RGBSpectrum& input)
    {
        RGBSpectrum result;
        for (int i = 0; i < 3; i++)
        	result.m_coefficients[i] = std::sqrt(input[i]);
        return result;
    }

	/*RGBSpectrum toRGBSpectrum() const
	{
		return *this;
	}*/
};

}