#pragma once
#include <gsl/span>

constexpr static int SIMD_WIDTH = 1;

namespace pandora::simd {
template <int S = SIMD_WIDTH>
class mask8;

template <typename T, int S>
class vec8_base;

template <typename T, int S>
class vec8;

#include "pandora/utility/simd/simd8_scalar.h"

using vec8_f32 = vec8<float, SIMD_WIDTH>;
using vec8_i32 = vec8<int, SIMD_WIDTH>;
}
