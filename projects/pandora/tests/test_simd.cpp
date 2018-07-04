#include "pandora/utility/simd/simd8.h"
#include <gtest/gtest.h>

using namespace pandora;

#define ASSERT_EQ_T(left, right)                               \
    {                                                          \
        if constexpr (std::is_same_v<float, decltype(left)>) { \
            ASSERT_FLOAT_EQ(left, right);                      \
        } else {                                               \
            ASSERT_EQ(left, right);                            \
        }                                                      \
    }

template <typename T, int S>
void simd8Tests()
{
    simd::vec8<T, S> v1(2);
    simd::vec8<T, S> v2(4, 5, 6, 7, 8, 9, 10, 11);

    {
        std::array<T, 8> values;
        v1.store(values);
        for (int i = 0; i < 8; i++) {
            ASSERT_EQ_T(values[i], (T)2);
        }
    }

    {
        std::array<T, 8> values;
        v2.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)4 + i);
    }

    {
        std::array<T, 8> values;
        auto v3 = v1 + v2;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)6 + i);
    }

    {
        std::array<T, 8> values;
        auto v3 = v1 - v2;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)-2 - i);
    }

    {
        std::array<T, 8> values;
        std::array<T, 8> values1;
        std::array<T, 8> values2;
        auto v3 = v1 * v2;
        v1.store(values1);
        v2.store(values2);
        v3.store(values);

        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)2 * (4 + i));
    }

    if constexpr (std::is_same_v<T, float>) {
        std::array<T, 8> values;
        auto v3 = v1 / v2;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)2 / (4 + i));
    }

    {
        auto v3 = v1 + v2;

        std::array<T, 8> values;
        simd::min(v2, v3).store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)4 + i);

        simd::max(v2, v3).store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)6 + i);
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        auto v3 = v2 << v1;
        std::array<T, 8> values;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (4 + i) << 2);
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        auto v3 = v2 >> v1;
        std::array<T, 8> values;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (4 + i) >> 2);
    }

    if constexpr (std::is_same_v<T, uint32_t>) {
        simd::vec8<uint32_t, S> mask(0xFF, 0x0F, 0xF0, 0x0, 0x1, 0x2, 0x3, 0xF0F0F);
        simd::vec8<uint32_t, S> source(123, 0xF3, 0xDD, 0xFFFFFFFF, 0b0101, 0b0101, 0xFF, 0xABCDE);
        std::array<uint32_t, 8> expectedResults = { 123, 0x3, 0xD0, 0x0, 0x1, 0x0, 0x3, 0xA0C0E };
        std::array<T, 8> values;
        (source & mask).store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], expectedResults[i]);
    }

    {
        simd::vec8<uint32_t, S> index(7, 6, 5, 4, 3, 2, 1, 0);
        auto v3 = v2.permute(index);
        std::array<T, 8> values;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (7 - i) + 4);
    }

    {
        simd::mask8<S> mask(false, false, true, false, true, true, true, true);
        ASSERT_EQ(mask.count(), 5);
    }

    {
        auto v3 = simd::vec8<T, S>(1, 42, 1, 1, 1, 42, 1, 42);
        simd::mask8<S> mask = v2 < v3;
        ASSERT_EQ(mask.count(), 3);
    }

    {
        auto v3 = simd::vec8<T, S>(1, 42, 1, 1, 1, 42, 1, 42);
        simd::mask8<S> mask = v2 > v3;
        ASSERT_EQ(mask.count(), 5);
    }

    {
        simd::mask8<S> mask(false, false, true, false, true, true, false, true);
        auto v3 = v2.compress(mask);
        std::array<T, 8> values;
        v3.store(values);
        ASSERT_EQ_T(values[0], 6);
        ASSERT_EQ_T(values[1], 8);
        ASSERT_EQ_T(values[2], 9);
        ASSERT_EQ_T(values[3], 11);
        ASSERT_EQ(mask.count(), 4);

		unsigned validMask = 0b10011001;
		ASSERT_EQ(mask.count(validMask), 2);
    }

	{
		simd::mask8<S> mask(false, false, true, false, true, true, false, true);
		auto v3 = v2.compress(mask);
		simd::vec8<uint32_t, S> indices(mask.computeCompressPermutation());
		auto v4 = v2.permute(indices);

		std::array<T, 8> valuesCompress;
		std::array<T, 8> valuesPermuteCompress;
		v3.store(valuesCompress);
		v4.store(valuesPermuteCompress);

		for (int i = 0; i < mask.count(); i++) {
			ASSERT_EQ(valuesCompress[i], valuesPermuteCompress[i]);
		}
	}
}

TEST(SIMD8, Scalar)
{
    simd8Tests<float, 1>();
    simd8Tests<uint32_t, 1>();
}

TEST(SIMD8, AVX2)
{
    simd8Tests<float, 8>();
    simd8Tests<uint32_t, 8>();
}
