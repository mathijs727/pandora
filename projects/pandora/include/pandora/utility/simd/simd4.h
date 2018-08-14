#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <emmintrin.h> // SSE2
#include <functional>
#include <gsl/span>
#include <immintrin.h> // AVX / AVX2
#include <iostream>
#include <nmmintrin.h> // Popcount
#include <xmmintrin.h> // SSE
#include <smmintrin.h> // SSE4.1

constexpr static int SIMD4_WIDTH = 1;

namespace pandora::simd {
template <int S>
class _mask4;

template <typename T, int S>
class _vec4_base;

template <typename T, int S>
class _vec4;

#include "pandora/utility/simd/simd4_avx2.h"
#include "pandora/utility/simd/simd4_scalar.h"

using mask4 = _mask4<SIMD4_WIDTH>;
using vec4_f32 = _vec4<float, SIMD4_WIDTH>;
//using vec4_i32 = vec4<int32_t, SIMD4_WIDTH>;
using vec4_u32 = _vec4<uint32_t, SIMD4_WIDTH>;
}
