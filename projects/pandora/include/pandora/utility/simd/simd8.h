#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <gsl/span>
#include <immintrin.h> // AVX / AVX2
#include <iostream>

constexpr static int SIMD8_WIDTH = 1;

namespace pandora::simd {
template <int S>
class _mask8;

template <typename T, int S>
class _vec8_base;

template <typename T, int S>
class _vec8;

#include "pandora/utility/simd/simd8_avx2.h"
#include "pandora/utility/simd/simd8_scalar.h"

using mask8 = _mask8<SIMD8_WIDTH>;
using vec8_f32 = _vec8<float, SIMD8_WIDTH>;
//using vec8_i32 = vec8<int32_t, SIMD8_WIDTH>;
using vec8_u32 = _vec8<uint32_t, SIMD8_WIDTH>;
}
