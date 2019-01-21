#include "simd/simd4.h"
#include <gtest/gtest.h>

using namespace simd;

#define ASSERT_EQ_T(left, right)                               \
    {                                                          \
        if constexpr (std::is_same_v<float, decltype(left)>) { \
            ASSERT_FLOAT_EQ(left, right);                      \
        } else {                                               \
            ASSERT_EQ(left, right);                            \
        }                                                      \
    }

template <typename T, int S>
void simd4Tests()
{
    simd::_vec4<T, S> v1(2);
    simd::_vec4<T, S> v2(4, 5, 6, 7);

    {
        std::array<T, 4> values;
        v1.store(values);
        for (int i = 0; i < 4; i++) {
            ASSERT_EQ_T(values[i], (T)2);
        }
    }

    {
        std::array<T, 4> values;
        v2.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)4 + i);
    }

    {
        std::array<T, 4> values;
        auto v3 = v1 + v2;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)6 + i);
    }

    {
        std::array<T, 4> values;
        auto v3 = v1 - v2;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)-2 - i);
    }

    if constexpr (std::is_same_v<T, float>) {
        {
            std::array<T, 4> values;
            auto v3 = -v2;
            v3.store(values);
            for (int i = 0; i < 4; i++)
                ASSERT_EQ_T(values[i], (T)(-4 - i));
        }

        {
            std::array<T, 4> values;
            auto v3 = -v2;
            auto v4 = v3.abs();
            v3.store(values);
            for (int i = 0; i < 4; i++)
                ASSERT_EQ_T(values[i], (T)(4 + i));
        }
    }

    {
        std::array<T, 4> values;
        std::array<T, 4> values1;
        std::array<T, 4> values2;
        auto v3 = v1 * v2;
        v1.store(values1);
        v2.store(values2);
        v3.store(values);

        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)2 * (4 + i));
    }

    if constexpr (std::is_same_v<T, float>) {
        std::array<T, 4> values;
        auto v3 = v1 / v2;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)2 / (4 + i));
    }

    {
        auto v3 = v1 + v2;

        std::array<T, 4> values;
        simd::min(v2, v3).store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)4 + i);

        simd::max(v2, v3).store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (T)6 + i);
    }

    {
        simd::_mask4<S> mask(false, false, true, true);
        auto v3 = simd::blend(v1, v2, mask);

        std::array<T, 4> values;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], i < 2 ? 2 : (4 + i));
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        auto v4 = simd::_vec4<T, S>(0, 1, 2, 3);
        auto v3 = v2 << v4;
        std::array<T, 4> values;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (4 + i) << (i % 4));
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        auto v4 = simd::_vec4<T, S>(1, 1, 2, 2);
        auto v3 = v2 >> v4;
        std::array<T, 4> values;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (4 + i) >> (1 + ((i / 2) % 2)));
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        auto v3 = v2 << 2;
        std::array<T, 4> values;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (4 + i) << 2);
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        auto v3 = v2 >> 2;
        std::array<T, 4> values;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], (4 + i) >> 2);
    }

    // Test bitwise operations
    if constexpr (std::is_same_v<T, uint32_t>) {
        // Bitwise AND
        {
            simd::_vec4<uint32_t, S> mask(0xFF, 0x0F, 0xF0, 0b000110);
            simd::_vec4<uint32_t, S> source(123, 0xF3, 0xDD, 0b101100);
            std::array<uint32_t, 4> expectedResults = { 255, 255, 253, 0b101110 };
            std::array<T, 4> values;
            (source & mask).store(values);
            for (int i = 0; i < 4; i++)
                ASSERT_EQ_T(values[i], expectedResults[i]);
        }

        // Bitwise OR
        {
            simd::_vec4<uint32_t, S> mask(0xFF, 0x0F, 0xF0, 0x0);
            simd::_vec4<uint32_t, S> source(123, 0xF3, 0xDD, 0xFFFFFFFF);
            std::array<uint32_t, 4> expectedResults = { 123, 0x3, 0xD0, 0x0 };
            std::array<T, 4> values;
            (source & mask).store(values);
            for (int i = 0; i < 4; i++)
                ASSERT_EQ_T(values[i], expectedResults[i]);
        }

        // Bitwise XOR
        {
            simd::_vec4<uint32_t, S> mask(0xFF, 0x0F, 0xF0, 0x0);
            simd::_vec4<uint32_t, S> source(123, 0xF3, 0xDD, 0xFFFFFFFF);
            std::array<uint32_t, 4> expectedResults = { 132, 252, 45, 4294967295 };
            std::array<T, 4> values;
            (source & mask).store(values);
            for (int i = 0; i < 4; i++)
                ASSERT_EQ_T(values[i], expectedResults[i]);
        }
    }

    // Test reinterpret casts
    {
        std::array<uint32_t, 4> source = { 0, 236454, 258348, 892333 };
        std::array<float, 4> dest;

        simd::_vec4<uint32_t, S> v1;
        v1.load(source);
        simd::_vec4<float, S> v2 = simd::intBitsToFloat(v1);
        v2.store(dest);

        static_assert(sizeof(float) == sizeof(uint32_t));
        std::memcmp(source.data(), dest.data(), 4 * sizeof(float));
    }
    {
        std::array<float, 4> source = { 0, 236454, 258348, 892333 };
        std::array<uint32_t, 4> dest;

        simd::_vec4<float, S> v1;
        v1.load(source);
        simd::_vec4<uint32_t, S> v2 = simd::floatBitsToInt(v1);
        v2.store(dest);

        static_assert(sizeof(float) == sizeof(uint32_t));
        std::memcmp(source.data(), dest.data(), 4 * sizeof(uint32_t));
    }

    {
        simd::_vec4<uint32_t, S> index(3, 2, 1, 0);
        auto v3 = v2.permute(index);
        std::array<T, 4> values;
        v3.store(values);
        for (int i = 0; i < 4; i++)
            ASSERT_EQ_T(values[i], 7 - i);
    }

    {
        simd::_mask4<S> mask(false, false, true, false);
        ASSERT_EQ(mask.bitMask(), 0b0100);
    }

    {
        simd::_mask4<S> mask(false, false, true, false);
        ASSERT_EQ(mask.count(), 1);
    }

    {
        simd::_mask4<S> mask1(true, false, true, false);
        ASSERT_TRUE(mask1.any());

        simd::_mask4<S> mask2(false, false, false, false);
        ASSERT_FALSE(mask2.any());
    }

    {
        simd::_mask4<S> mask1(true, false, true, false);
        ASSERT_FALSE(mask1.none());

        simd::_mask4<S> mask2(false, false, false, false);
        ASSERT_TRUE(mask2.none());
    }

    {
        simd::_mask4<S> mask1(true, false, true, false);
        ASSERT_FALSE(mask1.all());

        simd::_mask4<S> mask2(true, true, true, true);
        ASSERT_TRUE(mask2.all());
    }

    {
        auto v3 = simd::_vec4<T, S>(1, 42, 1, 7);
        simd::_mask4<S> mask = v2 < v3;
        ASSERT_EQ(mask.count(), 1);
    }

    {
        auto v3 = simd::_vec4<T, S>(1, 42, 1, 7);
        simd::_mask4<S> mask = v2 <= v3;
        ASSERT_EQ(mask.count(), 2);
    }

    {
        auto v3 = simd::_vec4<T, S>(1, 42, 1, 7);
        simd::_mask4<S> mask = v2 > v3;
        ASSERT_EQ(mask.count(), 2);
    }

    {
        auto v3 = simd::_vec4<T, S>(1, 42, 1, 7);
        simd::_mask4<S> mask = v2 >= v3;
        ASSERT_EQ(mask.count(), 3);
    }

    {
        simd::_mask4<S> mask1(true, false, true, false);
        simd::_mask4<S> mask2(true, true, true, true);

        simd::_mask4<S> andMask = mask1 && mask2;
        simd::_mask4<S> orMask = mask1 || mask2;

        ASSERT_TRUE(andMask.count() == 2);
        ASSERT_TRUE(orMask.count() == 4);
    }

    {
        simd::_mask4<S> mask(false, true, true, false);
        auto v3 = v2.compress(mask);
        std::array<T, 4> values;
        v3.store(values);
        ASSERT_EQ_T(values[0], 5);
        ASSERT_EQ_T(values[1], 6);
        ASSERT_EQ(mask.count(), 2);

        unsigned validMask = 0b1011;
        ASSERT_EQ(mask.count(validMask), 1);
    }

    {
        simd::_mask4<S> mask(false, false, true, false);
        auto v3 = v2.compress(mask);
        simd::_vec4<uint32_t, S> indices(mask.computeCompressPermutation());
        auto v4 = v2.permute(indices);

        std::array<T, 4> valuesCompress;
        std::array<T, 4> valuesPermuteCompress;
        v3.store(valuesCompress);
        v4.store(valuesPermuteCompress);

        for (int i = 0; i < mask.count(); i++) {
            ASSERT_EQ(valuesCompress[i], valuesPermuteCompress[i]);
        }
    }

    {
        simd::_vec4<T, S> values(5, 3, 7, 1);
        ASSERT_EQ(values.horizontalMin(), (T)1);
        ASSERT_EQ(values.horizontalMinIndex(), 3);

        ASSERT_EQ(values.horizontalMax(), (T)7);
        ASSERT_EQ(values.horizontalMaxIndex(), 2);
    }
}

TEST(SIMD4, Scalar)
{
    simd4Tests<float, 1>();
    simd4Tests<uint32_t, 1>();
}

TEST(SIMD4, AVX2)
{
    simd4Tests<uint32_t, 4>();
    simd4Tests<float, 4>();
}
