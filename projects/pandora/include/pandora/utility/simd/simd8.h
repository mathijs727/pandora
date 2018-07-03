#pragma once
#include <gsl/span>
#include <iostream>

constexpr static int SIMD_WIDTH = 8;

namespace pandora::simd {
template <int S = SIMD_WIDTH>
class mask8;

template <typename T, int S>
class vec8_base;

template <typename T, int S>
class vec8;

#include "pandora/utility/simd/simd8_avx2.h"
#include "pandora/utility/simd/simd8_scalar.h"

using vec8_f32 = vec8<float, SIMD_WIDTH>;
using vec8_i32 = vec8<int32_t, SIMD_WIDTH>;
using vec8_u32 = vec8<uint32_t, SIMD_WIDTH>;
}
