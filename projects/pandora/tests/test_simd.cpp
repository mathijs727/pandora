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
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (T)2);
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
        auto v3 = v1 * v2;
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

    {
		simd::vec8<uint32_t, S> index(7, 6, 5, 4, 3, 2, 1, 0);
        auto v3 = v2.permute(index);
        std::array<T, 8> values;
        v3.store(values);
        for (int i = 0; i < 8; i++)
            ASSERT_EQ_T(values[i], (7 - i) + 4);
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
    }

    {
        auto v3 = simd::vec8<T, S>(1, 42, 1, 1, 1, 42, 1, 42);
		simd::mask8<S> mask = v2 < v3;
		ASSERT_EQ(mask.count(), 3);
    }
}

TEST(SIMD8, Scalar)
{
	simd8Tests<float, 1>();
	simd8Tests<int, 1>();
}
