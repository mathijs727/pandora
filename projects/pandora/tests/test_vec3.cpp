#include "pandora/math/vec3.h"
#include "gtest/gtest.h"

using namespace pandora;

TEST(Vec3Test, ComparisonOperators)
{
    Vec3i a = Vec3i(8, 36, 64);
    Vec3i b = Vec3i(4, 2, 32);
    Vec3i c = Vec3i(4, 2, 32);

    ASSERT_NE(a, b);
    ASSERT_EQ(b, c);
}

TEST(Vec3Test, BinaryOperatorsInt)
{
    Vec3i a = Vec3i(8, 36, 64);
    Vec3i b = Vec3i(4, 2, 32);

    ASSERT_EQ(a + b, Vec3i(12, 38, 96));
    ASSERT_EQ(b + a, Vec3i(12, 38, 96));

    ASSERT_EQ(a - b, Vec3i(4, 34, 32));
    ASSERT_EQ(b - a, Vec3i(-4, -34, -32));

    ASSERT_EQ(a / b, Vec3i(2, 18, 2));
    // Dividing b by a would lead to rounding with integers

    ASSERT_EQ(a * b, Vec3i(32, 72, 2048));
    ASSERT_EQ(b * a, Vec3i(32, 72, 2048));
}

TEST(Vec3Test, BinaryOperatorsFloat)
{
    Vec3f a = Vec3f(5.0f, 2.0f, 3.0f);
    Vec3f b = Vec3f(9.0f, 11.0f, 4.0f);

    {
        Vec3f c = a / b;
        ASSERT_FLOAT_EQ(c.x, 5.0f / 9.0f);
        ASSERT_FLOAT_EQ(c.y, 2.0f / 11.0f);
        ASSERT_FLOAT_EQ(c.z, 3.0f / 4.0f);
    }

    {
        Vec3f c = 2.0f * a;
        ASSERT_FLOAT_EQ(c.x, 10.0f);
        ASSERT_FLOAT_EQ(c.y, 4.0f);
        ASSERT_FLOAT_EQ(c.z, 6.0f);
    }

    {
        Vec3f c = 30.0f / a;
        ASSERT_FLOAT_EQ(c.x, 6.0f);
        ASSERT_FLOAT_EQ(c.y, 15.0f);
        ASSERT_FLOAT_EQ(c.z, 10.0f);
    }

    {
        Vec3f c = 4.0f + b;
        ASSERT_FLOAT_EQ(c.x, 13.0f);
        ASSERT_FLOAT_EQ(c.y, 15.0f);
        ASSERT_FLOAT_EQ(c.z, 8.0f);
    }

    {
        Vec3f c = 10.0f - a;
        ASSERT_FLOAT_EQ(c.x, 5.0f);
        ASSERT_FLOAT_EQ(c.y, 8.0f);
        ASSERT_FLOAT_EQ(c.z, 7.0f);
    }
}

TEST(Vec3Test, UnaryOperators)
{
    Vec3i a = Vec3i(8, 36, 64);
    ASSERT_EQ(-a, Vec3i(-8, -36, -64));
    ASSERT_EQ(-(-a), a);
    ASSERT_EQ(-(-(-a)), -a);
}

TEST(Vec3Test, AssignmentOperators)
{
    Vec3i a = Vec3i(8, 36, 64);
    Vec3i b = Vec3i(4, 2, 32);

    {
        Vec3i c = a;
        c += b;
        ASSERT_EQ(c, a + b);
    }

    {
        Vec3i c = a;
        c -= b;
        ASSERT_EQ(c, a - b);
    }

    {
        Vec3i c = a;
        c /= b;
        ASSERT_EQ(c, a / b);
    }

    {
        Vec3i c = a;
        c *= b;
        ASSERT_EQ(c, a * b);
    }
}

TEST(Vec3Test, CrossProduct)
{
    Vec3f a = Vec3f(1.0f, 0.0f, 0.0f);
    Vec3f b = Vec3f(0.0f, 1.0f, 0.0f);
    Vec3f c = Vec3f(0.0f, 0.0f, 1.0f);

    Vec3f d = cross(a, b);
    ASSERT_FLOAT_EQ(d.x, 0.0f);
    ASSERT_FLOAT_EQ(d.y, 0.0f);
    ASSERT_FLOAT_EQ(d.z, 1.0f);

    Vec3f e = cross(c, b);
    ASSERT_FLOAT_EQ(e.x, -1.0f);
    ASSERT_FLOAT_EQ(e.y, 0.0f);
    ASSERT_FLOAT_EQ(e.z, 0.0f);

    Vec3f f = cross(a, -c);
    ASSERT_FLOAT_EQ(f.x, 0.0f);
    ASSERT_FLOAT_EQ(f.y, 1.0f);
    ASSERT_FLOAT_EQ(f.z, 0.0f);
}

TEST(Vec3Test, DotProduct)
{
    Vec3f a = Vec3f(2.0f, 4.0f, 5.0f);
    Vec3f b = Vec3f(3.0f, 6.0f, 7.0f);
    float c = dot(a, b);

    ASSERT_FLOAT_EQ(c, 65.0f);
}

TEST(Vec3Test, Abs)
{
    Vec3f a = Vec3f(-2.0f, -4.0f, -5.0f);
    Vec3f b = abs(a);
    ASSERT_FLOAT_EQ(b.x, 2.0f);
    ASSERT_FLOAT_EQ(b.y, 4.0f);
    ASSERT_FLOAT_EQ(b.z, 5.0f);
}

TEST(Vec3Test, Permute)
{
    Vec3f a = Vec3f(1.0f, 2.0f, 3.0f);
    Vec3f b = permute(a, 2, 1, 0);
    ASSERT_FLOAT_EQ(b.x, 3.0f);
    ASSERT_FLOAT_EQ(b.y, 2.0f);
    ASSERT_FLOAT_EQ(b.z, 1.0f);
}
