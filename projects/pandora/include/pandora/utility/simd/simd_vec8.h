#pragma once
#include <gsl/span>

namespace pandora {

template <int S>
class SIMDMask8;

template <typename T, int S>
class BaseSIMDVec8;

template <>
class SIMDMask8<1>
{
public:
	SIMDMask8() = default;
	SIMDMask8(bool v0, bool v1, bool v2, bool v3, bool v4, bool v5, bool v6, bool v7) {
		m_values[0] = v0;
		m_values[1] = v1;
		m_values[2] = v2;
		m_values[3] = v3;
		m_values[4] = v4;
		m_values[5] = v5;
		m_values[6] = v6;
		m_values[7] = v7;
	}

	inline int count() {
		int c = 0;
		for (bool v : m_values)
			if (v)
				c++;
		return c;
	}
private:
	std::array<bool, 8> m_values;

	template<typename T, int S> friend class BaseSIMDVec8;// Not possible to friend only BaseSIMDVec8<T, 1>
};

template <typename T, int S>
class SIMDVec8;

// Partial template specialization does not inherit members/functions from less precise specializations.
// Use a templated base class to inherit functions/members that are the same between different template specializations (i.e. floats & ints)
template <typename T>
class BaseSIMDVec8<T, 1> {
public:
    BaseSIMDVec8() = default;

    BaseSIMDVec8(gsl::span<const T, 8> v)
    {
        std::copy(std::begin(v), std::end(v), std::begin(m_values));
    }

    BaseSIMDVec8(T value)
    {
        std::fill(std::begin(m_values), std::end(m_values), value);
    }

    BaseSIMDVec8(T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7)
    {
        m_values[0] = v0;
        m_values[1] = v1;
        m_values[2] = v2;
        m_values[3] = v3;
        m_values[4] = v4;
        m_values[5] = v5;
        m_values[6] = v6;
        m_values[7] = v7;
    }

    inline void store(gsl::span<T, 8> v)
    {
        std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
    }

    inline SIMDVec8<T, 1> operator+(const SIMDVec8<T, 1>& other) const
    {
		SIMDVec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] + other.m_values[i];
        return result;
    }

    inline SIMDVec8<T, 1> operator-(const SIMDVec8<T, 1>& other) const
    {
		SIMDVec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] - other.m_values[i];
        return result;
    }

    inline SIMDVec8<T, 1> operator*(const SIMDVec8<T, 1>& other) const
    {
		SIMDVec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] * other.m_values[i];
        return result;
    }

	inline SIMDMask8<1> operator<(const SIMDVec8<T, 1>& other) const
	{
		SIMDMask8<1> mask;
		for (int i = 0; i < 8; i++)
			mask.m_values[i] = (m_values[i] < other.m_values[i]);
		return mask;
	}

    inline static SIMDVec8<T, 1> min(const SIMDVec8<T, 1>& a, const SIMDVec8<T, 1>& b)
    {
		SIMDVec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = std::min(a.m_values[i], b.m_values[i]);
        return result;
    }

    inline static SIMDVec8<T, 1> max(const SIMDVec8<T, 1>& a, const SIMDVec8<T, 1>& b)
    {
		SIMDVec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = std::max(a.m_values[i], b.m_values[i]);
        return result;
    }

	inline SIMDVec8<T, 1> permute(const SIMDVec8<int, 1>& index) const
	{
		SIMDVec8<T, 1> result;
		for (int i = 0; i < 8; i++) {
			result.m_values[i] = m_values[index.m_values[i]];
		}
		return result;
	}

	inline SIMDVec8<T, 1> compress(SIMDMask8<1> mask) const
	{
		SIMDVec8<T, 1> result;
		for (int i = 0, j = 0; i < 8; i++) {
			if (mask.m_values[i])
				result.m_values[j++] = m_values[i];
		}
		return result;
	}
protected:
    std::array<T, 8> m_values;
};

template <>
class SIMDVec8<float, 1> : public BaseSIMDVec8<float, 1> {
public:
	// Inherit constructors
	using BaseSIMDVec8<float, 1>::BaseSIMDVec8;

	// Not in base class because we will not support it on integers
    inline SIMDVec8<float, 1> operator/(const SIMDVec8<float, 1>& other) const
    {
        SIMDVec8<float, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] / other.m_values[i];
        return result;
    }
};


template <>
class SIMDVec8<int, 1> : public BaseSIMDVec8<int, 1> {
public:
	// Inherit constructors
	using BaseSIMDVec8<int, 1>::BaseSIMDVec8;
	friend class BaseSIMDVec8<float, 1>;// Make friend so it can access us in permute & compress operations

	inline SIMDVec8<int, 1> operator<<(const SIMDVec8<int, 1>& amount) const
	{
		SIMDVec8<int, 1> result;
		for (int i = 0; i < 8; i++) {
			result.m_values[i] = m_values[i] << amount.m_values[i];
		}
		return result;
	}

	inline SIMDVec8<int, 1> operator>>(const SIMDVec8<int, 1>& amount) const
	{
		SIMDVec8<int, 1> result;
		for (int i = 0; i < 8; i++) {
			result.m_values[i] = m_values[i] >> amount.m_values[i];
		}
		return result;
	}

};

}
