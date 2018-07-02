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

template <typename T, int S>
vec8<T, S> min(const vec8<T, S>& a, const vec8<T, S>& b);

template <typename T, int S>
vec8<T, S> max(const vec8<T, S>& a, const vec8<T, S>& b);

#include "pandora/utility/simd/simd8_scalar.h"

using vec8_f32 = vec8<float, SIMD_WIDTH>;
using vec8_i32 = vec8<int32_t, SIMD_WIDTH>;
using vec8_u32 = vec8<uint32_t, SIMD_WIDTH>;
}
