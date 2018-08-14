#include "pandora/utility/simd/simd4.h"
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
void simd4Tests()
{
	simd::vec4<T, S> v1(2);
	simd::vec4<T, S> v2(4, 5, 6, 7);

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

	if constexpr (std::is_same_v<T, uint32_t>) {
		auto v4 = simd::vec4<T, S>(0, 1, 2, 3);
		auto v3 = v2 << v4;
		std::array<T, 4> values;
		v3.store(values);
		for (int i = 0; i < 4; i++)
			ASSERT_EQ_T(values[i], (4 + i) << (i % 4));
	}

	if constexpr (std::is_same_v<T, uint32_t>) {
		auto v4 = simd::vec4<T, S>(1, 1, 2, 2);
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

	if constexpr (std::is_same_v<T, uint32_t>) {
		simd::vec4<uint32_t, S> mask(0xFF, 0x0F, 0xF0, 0x0);
		simd::vec4<uint32_t, S> source(123, 0xF3, 0xDD, 0xFFFFFFFF);
		std::array<uint32_t, 4> expectedResults = { 123, 0x3, 0xD0, 0x0 };
		std::array<T, 4> values;
		(source & mask).store(values);
		for (int i = 0; i < 4; i++)
			ASSERT_EQ_T(values[i], expectedResults[i]);
	}

	{
		simd::vec4<uint32_t, S> index(3, 2, 1, 0);
		auto v3 = v2.permute(index);
		std::array<T, 4> values;
		v3.store(values);
		for (int i = 0; i < 4; i++)
			ASSERT_EQ_T(values[i], 7 - i);
	}

	{
		simd::mask4<S> mask(false, false, true, false);
		ASSERT_EQ(mask.count(), 1);
	}

	{
		simd::mask4<S> mask1(true, false, true, false);
		ASSERT_TRUE(mask1.any());

		simd::mask4<S> mask2(false, false, false, false);
		ASSERT_FALSE(mask2.any());
	}

	{
		simd::mask4<S> mask1(true, false, true, false);
		ASSERT_FALSE(mask1.none());

		simd::mask4<S> mask2(false, false, false, false);
		ASSERT_TRUE(mask2.none());
	}

	{
		simd::mask4<S> mask1(true, false, true, false);
		ASSERT_FALSE(mask1.all());

		simd::mask4<S> mask2(true, true, true, true);
		ASSERT_TRUE(mask2.all());
	}

	{
		auto v3 = simd::vec4<T, S>(1, 42, 1, 7);
		simd::mask4<S> mask = v2 < v3;
		ASSERT_EQ(mask.count(), 1);
	}

	{
		auto v3 = simd::vec4<T, S>(1, 42, 1, 7);
		simd::mask4<S> mask = v2 <= v3;
		ASSERT_EQ(mask.count(), 2);
	}

	{
		auto v3 = simd::vec4<T, S>(1, 42, 1, 7);
		simd::mask4<S> mask = v2 > v3;
		ASSERT_EQ(mask.count(), 2);
	}

	{
		auto v3 = simd::vec4<T, S>(1, 42, 1, 7);
		simd::mask4<S> mask = v2 >= v3;
		ASSERT_EQ(mask.count(), 3);
	}

	{
		simd::mask4<S> mask1(true, false, true, false);
		simd::mask4<S> mask2(true, true, true, true);

		simd::mask4<S> andMask = mask1 && mask2;
		simd::mask4<S> orMask = mask1 || mask2;

		ASSERT_TRUE(andMask.count() == 2);
		ASSERT_TRUE(orMask.count() == 4);
	}

	{
		simd::mask4<S> mask(false, true, true, false);
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
		simd::mask4<S> mask(false, false, true, false);
		auto v3 = v2.compress(mask);
		simd::vec4<uint32_t, S> indices(mask.computeCompressPermutation());
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
		simd::vec4<T, S> values(5, 3, 7, 6);
		ASSERT_EQ(values.horizontalMin(), (T)3);
		ASSERT_EQ(values.horizontalMinPos(), 1);

		ASSERT_EQ(values.horizontalMax(), (T)7);
		ASSERT_EQ(values.horizontalMaxPos(), 2);
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
