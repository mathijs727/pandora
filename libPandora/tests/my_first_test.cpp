#include "gtest/gtest.h"

int add(int x, int y)
{
	return x + y;
}

TEST(AddTest, TwoAndTwo) {
	ASSERT_EQ(add(2, 2), 4);
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}