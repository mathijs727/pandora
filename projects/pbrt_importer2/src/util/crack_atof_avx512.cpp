#include "pbrt/util/crack_atof_avx512.h"
#include <array>
#include <cassert>
#include <charconv>
#include <emmintrin.h>
#include <immintrin.h>
#include <nmmintrin.h>

constexpr std::array<float, 32> computePowers(int center)
{
	std::array<float, 32> lut{};
	float r = 1.0f;
	for (int i = center - 1; i >= 0; i--) {
		lut[i] = r;
		r *= 10.0f;
	}

	r = 1.0f;
	for (int i = center; i < 32; i++) {
		lut[i] = r;
		r *= 0.1f;
	}
	lut[center] = 0.0f;

	return lut;
}

static float fallback(std::string_view string)
{
    // GNU standard library still has not implemented charconv
#if !defined(_WIN32)
    return static_cast<float>(std::atof(string.data()));
#else
    float value;
    std::from_chars(string.data(), string.data() + string.length(), value);
    return value;
#endif
}

float crack_atof_avx512(std::string_view string)
{
	const char* pBegin = string.data();
	int stringLength = static_cast<int>(string.length());
	float sign = 1.0f;

	// Take care of +/- sign
	if (*pBegin == '-') {
		++pBegin;
		--stringLength;
		sign = -1.0f;
	}
	else if (*pBegin == '+') {
		++pBegin;
		--stringLength;
	}

	if (stringLength > 16)
		return fallback(string);

	const __m128i raw_chars_epi8 = _mm_loadu_si128((const __m128i*)pBegin);
	const __m128i literalMask = _mm_mask_set1_epi8(_mm_setzero_si128(), 0xFFFF >> (16 - stringLength), (char)0xFF);
	const __m128i chars_epi8 = _mm_and_si128(_mm_sub_epi8(raw_chars_epi8, _mm_set1_epi8('0')), literalMask);

	const __m128i pointMask = _mm_set1_epi8('.');
	const int pointOffset = std::min(_mm_cmpistri(raw_chars_epi8, pointMask, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH), stringLength);

	// From 16 bytes (in SSE) to 16 ints (in AVX512)
	const __m512i chars_epi32 = _mm512_cvtepi8_epi32(chars_epi8);

	// Cast epi32 (int) to ps (float)
	const __m512 chars_ps = _mm512_cvtepi32_ps(chars_epi32);

	constexpr int powerLUTCenter = 16;
	static constexpr std::array<float, 32> powerLUT = computePowers(powerLUTCenter);

	// Compute partial results
	const int startOffset = powerLUTCenter - pointOffset;
	const __m512 powers = _mm512_loadu_ps(powerLUT.data() + startOffset);
	const __m512 partial = _mm512_mul_ps(powers, chars_ps);

	// Compute horizontal sum
	return sign * _mm512_reduce_add_ps(partial);
}