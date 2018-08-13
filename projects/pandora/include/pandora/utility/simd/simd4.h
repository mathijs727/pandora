#pragma once
#include <gsl/span>
#include <iostream>

constexpr static int SIMD4_WIDTH = 4;

namespace pandora::simd {
template <int S = SIMD4_WIDTH>
class mask4;

template <typename T, int S>
class vec4_base;

template <typename T, int S>
class vec4;

#include "pandora/utility/simd/simd4_avx2.h"
#include "pandora/utility/simd/simd4_scalar.h"

using vec4_f32 = vec4<float, SIMD4_WIDTH>;
using vec4_i32 = vec4<int32_t, SIMD4_WIDTH>;
using vec4_u32 = vec4<uint32_t, SIMD4_WIDTH>;
}
