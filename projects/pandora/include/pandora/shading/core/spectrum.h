#pragma once
#include <algorithm>
#include <array>

// Copied from PBRTv3
// https://github.com/mmp/pbrt-v3/blob/master/src/core/spectrum.h

namespace pandora {

template <int N>
class CoefficientSpectrum {
public:
	CoefficientSpectrum(float v = 0.0f)
	{
		std::fill(m_coefficients.begin(), m_coefficients.end(), v);
	}

    CoefficientSpectrum operator+=(const CoefficientSpectrum& other)
    {
        for (int i = 0; i < N; i++)
            m_coefficients[i] += other.m_coefficients[i];
        return *this;
    }

    CoefficientSpectrum operator+(const CoefficientSpectrum& other) const
    {
        CoefficientSpectrum<N> result = *this;
        for (int i = 0; i < N; i++)
            result.m_coefficients[i] += other.m_coefficients[i];
        return result;
    }

    CoefficientSpectrum operator-=(const CoefficientSpectrum& other)
	{
		for (int i = 0; i < N; i++)
			m_coefficients[i] -= other.m_coefficients[i];
		return *this;
	}

    CoefficientSpectrum operator-(const CoefficientSpectrum& other) const
	{
		CoefficientSpectrum<N> result = *this;
		for (int i = 0; i < N; i++)
			result.m_coefficients[i] -= other.m_coefficients[i];
		return result;
	}

    CoefficientSpectrum operator*=(const CoefficientSpectrum& other)
    {
        for (int i = 0; i < N; i++)
            m_coefficients[i] *= other.m_coefficients[i];
        return *this;
    }

    CoefficientSpectrum operator*(const CoefficientSpectrum& other) const
    {
        CoefficientSpectrum<N> result = *this;
        for (int i = 0; i < N; i++)
            result.m_coefficients[i] *= other.m_coefficients[i];
        return result;
    }

    friend CoefficientSpectrum operator*(float a, const CoefficientSpectrum& b)
    {
        CoefficientSpectrum<N> result;
        for (int i = 0; i < N; i++)
            result.m_coefficients[i] = a * b.m_coefficients[i];
        return result;
    }

    CoefficientSpectrum operator/=(const CoefficientSpectrum& other)
    {
        for (int i = 0; i < N; i++)
            m_coefficients[i] /= other.m_coefficients[i];
        return *this;
    }

    CoefficientSpectrum operator/(const CoefficientSpectrum& other) const
    {
        CoefficientSpectrum<N> result = *this;
        for (int i = 0; i < N; i++)
            result.m_coefficients[i] /= other.m_coefficients[i];
        return result;
    }

	bool isBlack() const
	{
		for (int i = 0; i < N; i++)
			if (m_coefficients[i] == 0.0f)
				return False;
		return true;
	}

	bool hasNans() const
	{
		for (int i = 0; i < N; i++)
			if (std::isnan(m_coefficients[i])) return true;
		return false;
	}

	/*friend CoefficientSpectrum sqrt(const CoefficientSpectrum& input)
	{
		CoefficientSpectrum<N> result;
		//for (int i = 0; i < N; i++)
		//	result[i] = std::sqrt(other.m_coefficients[i]);
		std::transform(
			input.m_coefficients.begin(),
			input.m_coefficients.end(),
			result.m_coefficients.begin(),
			std::sqrt);
		return result;
	}*/

	float operator[](int i) const
	{
		return m_coefficients[i];
	}

	float& operator[](int i)
	{
		return m_coefficients[i];
	}
protected:
	std::array<float, N> m_coefficients;
};

}