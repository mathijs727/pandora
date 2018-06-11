#include "glm/glm.hpp"
#include "gtest/gtest.h"

using namespace pandora;

TEST(Vec3Test, ComparisonOperators)
{
    glm::ivec3 a = glm::ivec3(8, 36, 64);
    glm::ivec3 b = glm::ivec3(4, 2, 32);
    glm::ivec3 c = glm::ivec3(4, 2, 32);

    ASSERT_NE(a, b);
    ASSERT_EQ(b, c);
}

TEST(Vec3Test, BinaryOperatorsInt)
{
    glm::ivec3 a = glm::ivec3(8, 36, 64);
    glm::ivec3 b = glm::ivec3(4, 2, 32);

    ASSERT_EQ(a + b, glm::ivec3(12, 38, 96));
    ASSERT_EQ(b + a, glm::ivec3(12, 38, 96));

    ASSERT_EQ(a - b, glm::ivec3(4, 34, 32));
    ASSERT_EQ(b - a, glm::ivec3(-4, -34, -32));

    ASSERT_EQ(a / b, glm::ivec3(2, 18, 2));
    // Dividing b by a would lead to rounding with integers

    ASSERT_EQ(a * b, glm::ivec3(32, 72, 2048));
    ASSERT_EQ(b * a, glm::ivec3(32, 72, 2048));
}

TEST(Vec3Test, BinaryOperatorsFloat)
{
    glm::vec3 a = glm::vec3(5.0f, 2.0f, 3.0f);
    glm::vec3 b = glm::vec3(9.0f, 11.0f, 4.0f);

    {
        glm::vec3 c = a / b;
        ASSERT_FLOAT_EQ(c.x, 5.0f / 9.0f);
        ASSERT_FLOAT_EQ(c.y, 2.0f / 11.0f);
        ASSERT_FLOAT_EQ(c.z, 3.0f / 4.0f);
    }

    {
        glm::vec3 c = 2.0f * a;
        ASSERT_FLOAT_EQ(c.x, 10.0f);
        ASSERT_FLOAT_EQ(c.y, 4.0f);
        ASSERT_FLOAT_EQ(c.z, 6.0f);
    }

    {
        glm::vec3 c = 30.0f / a;
        ASSERT_FLOAT_EQ(c.x, 6.0f);
        ASSERT_FLOAT_EQ(c.y, 15.0f);
        ASSERT_FLOAT_EQ(c.z, 10.0f);
    }

    {
        glm::vec3 c = 4.0f + b;
        ASSERT_FLOAT_EQ(c.x, 13.0f);
        ASSERT_FLOAT_EQ(c.y, 15.0f);
        ASSERT_FLOAT_EQ(c.z, 8.0f);
    }

    {
        glm::vec3 c = 10.0f - a;
        ASSERT_FLOAT_EQ(c.x, 5.0f);
        ASSERT_FLOAT_EQ(c.y, 8.0f);
        ASSERT_FLOAT_EQ(c.z, 7.0f);
    }
}

TEST(Vec3Test, UnaryOperators)
{
    glm::ivec3 a = glm::ivec3(8, 36, 64);
    ASSERT_EQ(-a, glm::ivec3(-8, -36, -64));
    ASSERT_EQ(-(-a), a);
    ASSERT_EQ(-(-(-a)), -a);
}

TEST(Vec3Test, AssignmentOperators)
{
    glm::ivec3 a = glm::ivec3(8, 36, 64);
    glm::ivec3 b = glm::ivec3(4, 2, 32);

    {
        glm::ivec3 c = a;
        c += b;
        ASSERT_EQ(c, a + b);
    }

    {
        glm::ivec3 c = a;
        c -= b;
        ASSERT_EQ(c, a - b);
    }

    {
        glm::ivec3 c = a;
        c /= b;
        ASSERT_EQ(c, a / b);
    }

    {
        glm::ivec3 c = a;
        c *= b;
        ASSERT_EQ(c, a * b);
    }
}

TEST(Vec3Test, CrossProduct)
{
    glm::vec3 a = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 b = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 c = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec3 d = cross(a, b);
    ASSERT_FLOAT_EQ(d.x, 0.0f);
    ASSERT_FLOAT_EQ(d.y, 0.0f);
    ASSERT_FLOAT_EQ(d.z, 1.0f);

    glm::vec3 e = cross(c, b);
    ASSERT_FLOAT_EQ(e.x, -1.0f);
    ASSERT_FLOAT_EQ(e.y, 0.0f);
    ASSERT_FLOAT_EQ(e.z, 0.0f);

    glm::vec3 f = cross(a, -c);
    ASSERT_FLOAT_EQ(f.x, 0.0f);
    ASSERT_FLOAT_EQ(f.y, 1.0f);
    ASSERT_FLOAT_EQ(f.z, 0.0f);
}

TEST(Vec3Test, DotProduct)
{
    glm::vec3 a = glm::vec3(2.0f, 4.0f, 5.0f);
    glm::vec3 b = glm::vec3(3.0f, 6.0f, 7.0f);
    float c = dot(a, b);

    ASSERT_FLOAT_EQ(c, 65.0f);
}

TEST(Vec3Test, Abs)
{
    glm::vec3 a = glm::vec3(-2.0f, -4.0f, -5.0f);
    glm::vec3 b = abs(a);
    ASSERT_FLOAT_EQ(b.x, 2.0f);
    ASSERT_FLOAT_EQ(b.y, 4.0f);
    ASSERT_FLOAT_EQ(b.z, 5.0f);
}

TEST(Vec3Test, Permute)
{
    glm::vec3 a = glm::vec3(1.0f, 2.0f, 3.0f);
    glm::vec3 b = permute(a, 2, 1, 0);
    ASSERT_FLOAT_EQ(b.x, 3.0f);
    ASSERT_FLOAT_EQ(b.y, 2.0f);
    ASSERT_FLOAT_EQ(b.z, 1.0f);
}
