#include "pbrt/util/crack_atof_sse.h"
#include <array>
#include <cassert>
#include <charconv>
#include <emmintrin.h>
#include <immintrin.h>
#include <nmmintrin.h>

constexpr std::array<float, 32> computePowers(int center)
{
    std::array<float, 32> lut {};
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
    float value { 0.0f };
    std::from_chars(string.data(), string.data() + string.length(), value);
    return value;
}

float crack_atof_sse(std::string_view string)
{
    const char* pBegin = string.data();
    int stringLength = static_cast<int>(string.length());
    float sign = 1.0f;

    // Take care of +/- sign
    if (*pBegin == '-') {
        ++pBegin;
        --stringLength;
        sign = -1.0f;
    } else if (*pBegin == '+') {
        ++pBegin;
        --stringLength;
    }

    if (stringLength > 16)
        return fallback(string);

    const __m128i raw_chars_epi8 = _mm_loadu_si128((const __m128i*)pBegin);
    const uint64_t part1 = 0xFFFFFFFFFFFFFFFF >> (std::max(0, 8 - stringLength) * 8);
    const uint64_t part2 = stringLength <= 8 ? 0x0 : (0xFFFFFFFFFFFFFFFF >> ((16 - stringLength) * 8));
    const __m128i literalMask = _mm_set_epi64x(part2, part1);
    const __m128i chars_epi8 = _mm_and_si128(_mm_sub_epi8(raw_chars_epi8, _mm_set1_epi8('0')), literalMask);

    const __m128i pointMask = _mm_set1_epi8('.'); //_mm_set_epi8('.', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e');
    const int pointOffset = std::min(_mm_cmpistri(raw_chars_epi8, pointMask, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH), stringLength);
    //if (pBegin[pointOffset] == 'e')
    //    return fallback(string);

    // Unpack epi8 => epi16
    const __m128i chars_epi16_1 = _mm_unpacklo_epi8(chars_epi8, _mm_setzero_si128());
    const __m128i chars_epi16_2 = _mm_unpackhi_epi8(chars_epi8, _mm_setzero_si128());

    // Unpack epi16 => epi32
    const __m128i chars_epi32_1 = _mm_unpacklo_epi16(chars_epi16_1, _mm_setzero_si128());
    const __m128i chars_epi32_2 = _mm_unpackhi_epi16(chars_epi16_1, _mm_setzero_si128());
    const __m128i chars_epi32_3 = _mm_unpacklo_epi16(chars_epi16_2, _mm_setzero_si128());
    const __m128i chars_epi32_4 = _mm_unpackhi_epi16(chars_epi16_2, _mm_setzero_si128());
    
	// Slightly slower than unpacking...
    /*const __m128i chars_epi32_1 = _mm_cvtepi8_epi32(chars_epi8);
    const __m128i chars_epi32_2 = _mm_cvtepi8_epi32(_mm_srli_si128(chars_epi8, 4));
    const __m128i chars_epi32_3 = _mm_cvtepi8_epi32(_mm_srli_si128(chars_epi8, 8));
    const __m128i chars_epi32_4 = _mm_cvtepi8_epi32(_mm_srli_si128(chars_epi8, 12));*/

    // Cast epi32 (int) to ps (float)
    const __m128 chars_ps_1 = _mm_cvtepi32_ps(chars_epi32_1);
    const __m128 chars_ps_2 = _mm_cvtepi32_ps(chars_epi32_2);
    const __m128 chars_ps_3 = _mm_cvtepi32_ps(chars_epi32_3);
    const __m128 chars_ps_4 = _mm_cvtepi32_ps(chars_epi32_4);

    constexpr int powerLUTCenter = 16;
    static constexpr std::array<float, 32> powerLUT = computePowers(powerLUTCenter);

    // Compute the sum
    const int startOffset = powerLUTCenter - pointOffset;
    const __m128 powers_1 = _mm_loadu_ps(powerLUT.data() + startOffset + 0);
    const __m128 sum_1 = _mm_mul_ps(powers_1, chars_ps_1);
    const __m128 powers_2 = _mm_loadu_ps(powerLUT.data() + startOffset + 4);
    const __m128 sum_2 = _mm_mul_ps(powers_2, chars_ps_2);
    const __m128 powers_3 = _mm_loadu_ps(powerLUT.data() + startOffset + 8);
    const __m128 sum_13 = _mm_fmadd_ps(powers_3, chars_ps_3, sum_1);
    const __m128 powers_4 = _mm_loadu_ps(powerLUT.data() + startOffset + 12);
    const __m128 sum_24 = _mm_fmadd_ps(powers_4, chars_ps_4, sum_2);
    const __m128 sum = _mm_add_ps(sum_13, sum_24);

    // Horizontal sum
    const __m128 hsum = _mm_hadd_ps(sum, sum);

    alignas(16) float sumData[4];
    _mm_store_ps(sumData, hsum);

    return sign * (sumData[0] + sumData[1]); // Final addition in scalar is faster than an extra _mm_hadd_ps()
}