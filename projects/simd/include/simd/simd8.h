#pragma once
#include "intrinsics.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <gsl/span>
#include <immintrin.h> // AVX / AVX2
#include <iostream>

#ifdef NDEBUG
constexpr static int SIMD8_WIDTH = 8;
#else
constexpr static int SIMD8_WIDTH = 8;
#endif

namespace simd {
template <int S>
class _mask8;

template <typename T, int S>
class _vec8_base;

template <typename T, int S>
class _vec8;

#include "simd/simd8_avx2.ipp"
#include "simd/simd8_scalar.ipp"

using mask8 = _mask8<SIMD8_WIDTH>;
using vec8_f32 = _vec8<float, SIMD8_WIDTH>;
//using vec8_i32 = vec8<int32_t, SIMD8_WIDTH>;
using vec8_u32 = _vec8<uint32_t, SIMD8_WIDTH>;
}
