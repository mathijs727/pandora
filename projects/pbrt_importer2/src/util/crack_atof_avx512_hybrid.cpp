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
	float value;
	std::from_chars(string.data(), string.data() + string.length(), value);
	return value;
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

	//const __m128i literalMask = _mm_mask_set1_epi8(_mm_setzero_si128(), 0xFFFF >> (16 - stringLength), (char)0xFF);
	const __m128i raw_chars_epi8 = _mm_loadu_si128((const __m128i*)pBegin);
	const __mmask16 literalMask = 0xFFFF >> (16 - stringLength);
	const __m128i chars_epi8 = _mm_mask_sub_epi8(_mm_setzero_si128(), literalMask, raw_chars_epi8, _mm_set1_epi8('0'));

	const __m128i pointMask = _mm_set1_epi8('.');
	const int pointOffset = std::min(_mm_cmpistri(raw_chars_epi8, pointMask, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH), stringLength);

	// Use 8-wide instructions so we don't tank clock speeds and to reduce the size of the horizontal sum at the end
	const __m256i chars_epi32_1 = _mm256_cvtepi8_epi32(chars_epi8);
	const __m256i chars_epi32_2 = _mm256_cvtepi8_epi32(_mm_srli_si128(chars_epi8, 8));

	// Cast epi32 (int) to ps (float)
	const __m256 chars_ps_1 = _mm256_cvtepi32_ps(chars_epi32_1);
	const __m256 chars_ps_2 = _mm256_cvtepi32_ps(chars_epi32_2);

	constexpr int powerLUTCenter = 16;
	static constexpr std::array<float, 32> powerLUT = computePowers(powerLUTCenter);

	// Compute partial results
	const int startOffset = powerLUTCenter - pointOffset;
	const __m256 powers_1 = _mm256_loadu_ps(powerLUT.data() + startOffset + 0);
	const __m256 sum_1 = _mm256_mul_ps(powers_1, chars_ps_1);
	const __m256 powers_2 = _mm256_loadu_ps(powerLUT.data() + startOffset + 8);
	// FMA faster than regular sum on Intel 9800X (but slower on AMD 3900X)?
	const __m256 sum = _mm256_fmadd_ps(powers_2, chars_ps_2, sum_1);

	// Horizontal sum
	// https://stackoverflow.com/questions/9775538/fastest-way-to-do-horizontal-vector-sum-with-avx-instructions
	const __m256 hsum8 = _mm256_hadd_ps(sum, sum);
	const __m128 hsum4 = _mm_add_ps(_mm256_extractf128_ps(hsum8, 1), _mm256_castps256_ps128(hsum8)); // Add upper and lower 4 lanes

	alignas(16) float sumData[4];
	_mm_store_ps(sumData, hsum4);

	return sign * (sumData[0] + sumData[1]);
}