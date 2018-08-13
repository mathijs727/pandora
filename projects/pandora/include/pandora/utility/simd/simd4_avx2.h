#pragma once
#include <cstdint>
#include <emmintrin.h> // SSE2
#include <gsl/span>
#include <nmmintrin.h> // Popcount
#include <xmmintrin.h> // SSE
#include <immintrin.h> // AVX / AVX2

constexpr std::array<uint32_t, 16> genCompressLUT4()
{
    std::array<uint32_t, 16> result = {};
    for (uint32_t mask = 0; mask < 16; mask++) {
        uint32_t indices = 0; // Permutation indices
        uint32_t k = 0;
        for (uint32_t bit = 0; bit < 8; bit++) {
            if ((mask & (1ull << bit)) != 0) { // If bit is set
                indices |= bit << k; // Add to permutation indices
                k += 8; // One byte
            }
        }
        result[mask] = indices;
    }
    return result;
}
constexpr std::array<uint32_t, 16> s_sseIndicesLUT = genCompressLUT4();
const __m128i s_sseMaskOnes = _mm_set1_epi32(0xFFFFFFFF);

template <typename T, int S>
class vec4;

template <int S>
class mask4;

template <typename T>
vec4<T, 4> min(const vec4<T, 4>& a, const vec4<T, 4>& b);

template <typename T>
vec4<T, 4> max(const vec4<T, 4>& a, const vec4<T, 4>& b);

template <>
class alignas(16) mask4<4> {
public:
    //mask4() = default;
	explicit inline mask4(const __m128i& value) :
		m_value(value),
		m_bitMask(_mm_movemask_ps(_mm_castsi128_ps(m_value)))
	{
	}
	explicit inline mask4(const __m128& value) :
		m_value(_mm_castps_si128(value)),
		m_bitMask(_mm_movemask_ps(value))
	{
	}
	mask4(bool v0, bool v1, bool v2, bool v3)
	{
        m_value = _mm_set_epi32(
            v0 ? 0xFFFFFFFF : 0x0,
            v1 ? 0xFFFFFFFF : 0x0,
            v2 ? 0xFFFFFFFF : 0x0,
            v3 ? 0xFFFFFFFF : 0x0);
        m_bitMask = _mm_movemask_ps(_mm_castsi128_ps(m_value));
	}

	inline int count(unsigned validMask) const
	{
        uint32_t mask = m_bitMask & validMask;
        return _mm_popcnt_u32(mask);
	}

	inline int count() const
	{
        return _mm_popcnt_u32(m_bitMask);
	}

	inline __m128i computeCompressPermutation() const
	{
        uint32_t wantedIndices = s_sseIndicesLUT[m_bitMask];
        return _mm_cvtepu8_epi32(_mm_cvtsi32_si128(wantedIndices));
	}

private:
	__m128i m_value;
	unsigned m_bitMask;

	template <typename T, int S>
	friend class vec4;
};

template <>
class vec4<uint32_t, 4> {
public:
    friend class vec4<float, 4>; // Make friend so it can access us in permute & compress operations

	vec4() = default;
	explicit inline vec4(gsl::span<const uint32_t, 4> v)
	{
		load(v);
	}
	explicit inline vec4(uint32_t value)
	{
		broadcast(value);
	}
	explicit inline vec4(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
	{
		m_value = _mm_set_epi32(v3, v2, v1, v0);
	}
	explicit inline vec4(__m128i value)
		: m_value(value)
	{
	}

	inline void loadAligned(gsl::span<const uint32_t, 4> v)
	{
		m_value = _mm_load_si128(reinterpret_cast<const __m128i*>(v.data()));
	}

	inline void storeAligned(gsl::span<uint32_t, 4> v) const
	{
		_mm_store_si128(reinterpret_cast<__m128i*>(v.data()), m_value);
	}

	inline void load(gsl::span<const uint32_t, 4> v)
	{
		m_value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data()));
	}

	inline void store(gsl::span<uint32_t, 4> v) const
	{
		_mm_storeu_si128(reinterpret_cast<__m128i*>(v.data()), m_value);
	}

	inline void broadcast(uint32_t value)
	{
		m_value = _mm_set1_epi32(value);
	}

	inline vec4<uint32_t, 4> operator+(const vec4<uint32_t, 4>& other) const
	{
		return vec4(_mm_add_epi32(m_value, other.m_value));
	}

	inline vec4<uint32_t, 4> operator-(const vec4<uint32_t, 4>& other) const
	{
		return vec4(_mm_sub_epi32(m_value, other.m_value));
	}

	inline vec4<uint32_t, 4> operator*(const vec4<uint32_t, 4>& other) const
	{
		return vec4(_mm_mullo_epi32(m_value, other.m_value));
	}

	// No integer divide (not part of SSE/AVX anyways)

	inline vec4<uint32_t, 4> operator<<(const vec4<uint32_t, 4>& amount) const
	{
		// AVX2 functionality
		return vec4(_mm_sllv_epi32(m_value, amount.m_value));
	}

	inline vec4<uint32_t, 4> operator<<(uint32_t amount) const
	{
		return vec4(_mm_slli_epi32(m_value, amount));
	}

	inline vec4<uint32_t, 4> operator>>(const vec4<uint32_t, 4>& amount) const
	{
		// AVX2 functionality
		return vec4(_mm_srlv_epi32(m_value, amount.m_value));
	}

	inline vec4<uint32_t, 4> operator>>(uint32_t amount) const
	{
		return vec4(_mm_srli_epi32(m_value, amount));
	}

	inline vec4<uint32_t, 4> operator&(const vec4<uint32_t, 4>& other) const
	{
		return vec4(_mm_and_si128(m_value, other.m_value));
	}

	inline mask4<4> operator<(const vec4<uint32_t, 4>& other) const
	{
		return mask4<4>(_mm_cmplt_epi32(m_value, other.m_value));
	}

	inline mask4<4> operator<=(const vec4<uint32_t, 4>& other) const
	{
		__m128i greaterThan = _mm_cmpgt_epi32(m_value, other.m_value);
		__m128i notGreaterThan = _mm_xor_si128(greaterThan, s_sseMaskOnes);
		return mask4<4>(notGreaterThan);
	}

	inline mask4<4> operator>(const vec4<uint32_t, 4>& other) const
	{
		return mask4<4>(_mm_cmpgt_epi32(m_value, other.m_value));
	}

	inline mask4<4> operator>=(const vec4<uint32_t, 4>& other) const
	{
		__m128i lessThan = _mm_cmpgt_epi32(other.m_value, m_value);
		__m128i notLessThan = _mm_xor_si128(lessThan, s_sseMaskOnes);
		return mask4<4>(notLessThan);
	}

	inline vec4<uint32_t, 4> permute(const vec4<uint32_t, 4>& index) const
	{
		// AVX functionality
		return vec4(_mm_castps_si128(_mm_permutevar_ps(_mm_castsi128_ps(m_value), index.m_value)));
	}

	inline vec4<uint32_t, 4> compress(const mask4<4>& mask) const
	{
		__m128i shuffleMask = mask.computeCompressPermutation();
		return permute(vec4(shuffleMask));
	}
	friend vec4<uint32_t, 4> min(const vec4<uint32_t, 4>& a, const vec4<uint32_t, 4>& b);
	friend vec4<uint32_t, 4> max(const vec4<uint32_t, 4>& a, const vec4<uint32_t, 4>& b);

private:
	__m128i m_value;
};

template <>
class alignas(16) vec4<float, 4> {
public:
	vec4() = default;
	inline vec4(gsl::span<const float, 4> v)
	{
		load(v);
	}
	inline vec4(float value)
	{
		broadcast(value);
	}
	inline vec4(float v0, float v1, float v2, float v3)
	{
		m_value = _mm_set_ps(v3, v2, v1, v0);
	}
	explicit inline vec4(__m128 value)
		: m_value(value)
	{
	}

	inline void loadAligned(gsl::span<const float, 4> v)
	{
		m_value = _mm_load_ps(v.data());
	}

	inline void storeAligned(gsl::span<float, 4> v) const
	{
		_mm_store_ps(v.data(), m_value);
	}

	inline void load(gsl::span<const float, 4> v)
	{
		m_value = _mm_loadu_ps(v.data());
	}

	inline void store(gsl::span<float, 4> v) const
	{
		_mm_storeu_ps(v.data(), m_value);
	}

	inline void broadcast(float value)
	{
		m_value = _mm_set_ps1(value);
	}

	inline vec4<float, 4> operator+(const vec4<float, 4>& other) const
	{
		return vec4(_mm_add_ps(m_value, other.m_value));
	}

	inline vec4<float, 4> operator-(const vec4<float, 4>& other) const
	{
		return vec4(_mm_sub_ps(m_value, other.m_value));
	}

	inline vec4<float, 4> operator*(const vec4<float, 4>& other) const
	{
		return vec4(_mm_mul_ps(m_value, other.m_value));
	}

	inline vec4<float, 4> operator/(const vec4<float, 4>& other) const
	{
		return vec4(_mm_div_ps(m_value, other.m_value));
	}

	inline mask4<4> operator<(const vec4<float, 4>& other) const
	{
		return mask4<4>(_mm_cmplt_ps(m_value, other.m_value));
	}

	inline mask4<4> operator<=(const vec4<float, 4>& other) const
	{
		return mask4<4>(_mm_cmple_ps(m_value, other.m_value));
	}

	inline mask4<4> operator>(const vec4<float, 4>& other) const
	{
		return mask4<4>(_mm_cmpgt_ps(m_value, other.m_value));
	}

	inline mask4<4> operator>=(const vec4<float, 4>& other) const
	{
		return mask4<4>(_mm_cmpge_ps(m_value, other.m_value));
	}

	inline vec4<float, 4> permute(const vec4<uint32_t, 4>& index) const
	{
		// AVX functionality
		return vec4(_mm_permutevar_ps(m_value, index.m_value));
	}

	inline vec4<float, 4> compress(const mask4<4>& mask) const
	{
		__m128i shuffleMask = mask.computeCompressPermutation();
		return permute(vec4<uint32_t, 4>(shuffleMask));
	}
	friend vec4<float, 4> min(const vec4<float, 4>& a, const vec4<float, 4>& b);
	friend vec4<float, 4> max(const vec4<float, 4>& a, const vec4<float, 4>& b);

private:
	__m128 m_value;
};


inline vec4<uint32_t, 4> min(const vec4<uint32_t, 4>& a, const vec4<uint32_t, 4>& b)
{
	return vec4<uint32_t, 4>(_mm_min_epu32(a.m_value, b.m_value));
}

inline vec4<uint32_t, 4> max(const vec4<uint32_t, 4>& a, const vec4<uint32_t, 4>& b)
{
	return vec4<uint32_t, 4>(_mm_max_epu32(a.m_value, b.m_value));
}

inline vec4<float, 4> min(const vec4<float, 4>& a, const vec4<float, 4>& b)
{
	return vec4<float, 4>(_mm_min_ps(a.m_value, b.m_value));
}

inline vec4<float, 4> max(const vec4<float, 4>& a, const vec4<float, 4>& b)
{
	return vec4<float, 4>(_mm_max_ps(a.m_value, b.m_value));
}

