#include "pandora/math/constants.h"
#include "pandora/math/mat3x4.h"
#include "pandora/math/quaternion.h"
#include "pandora/math/transform.h"
#include "pandora/math/vec3.h"
#include "gtest/gtest.h"
#include <iostream>

using namespace pandora;

TEST(TransformTest, Quaternion1)
{
    Vec3f xAxis = Vec3f(1.0f, 0.0f, 0.0f);
    Vec3f yAxis = Vec3f(0.0f, 1.0f, 0.0f);
    Vec3f zAxis = Vec3f(0.0f, 0.0f, 1.0f);

    QuatF rotation1 = QuatF::rotation(xAxis, piF); //          0,  0, -1
    QuatF rotation2 = QuatF::rotation(-yAxis, piF / 2.0f); //  1,  0,  0
    QuatF finalRotation = rotation2 * rotation1;

    // ASSERT_FLOAT_EQ does not except the rounding errors
    Vec3f rotatedVector = finalRotation.rotateVector(zAxis);
    ASSERT_NEAR(rotatedVector.x, 1.0f, 10e-7f);
    ASSERT_NEAR(rotatedVector.y, 0.0f, 10e-7f);
    ASSERT_NEAR(rotatedVector.z, 0.0f, 10e-7f);
}

TEST(TransformTest, Quaternion2)
{
    Vec3f xAxis = Vec3f(1.0f, 0.0f, 0.0f);
    Vec3f yAxis = Vec3f(0.0f, 1.0f, 0.0f);
    Vec3f zAxis = Vec3f(0.0f, 0.0f, 1.0f);

    QuatF rotation1 = QuatF::rotation(zAxis, piF * 1.5f); //  0, -1,  0
    QuatF rotation2 = QuatF::rotation(xAxis, piF * 0.5f); //  0,  0, -1
    QuatF rotation3 = QuatF::rotation(yAxis, piF * 1.5f); //  1,  0,  0
    QuatF finalRotation = rotation3 * rotation2 * rotation1;

    Vec3f rotatedVector = finalRotation.rotateVector(xAxis);
    ASSERT_FLOAT_EQ(rotatedVector.x, 1.0f);
    ASSERT_FLOAT_EQ(rotatedVector.y, 0.0f);
    ASSERT_FLOAT_EQ(rotatedVector.z, 0.0f);
}

TEST(TransformTest, RotationMatrix1)
{
    TransformFloat myTransform;
    myTransform.rotateX(piF / 2.0f); //   0, -1, 0
    myTransform.rotateZ(piF / 2.0f); //  1,  0,  0

    Mat3x4f matrix = myTransform.getTransformMatrix();
    Vec3f matRotatedVector = matrix.transformVector(Vec3f(0.0f, 0.0f, 1.0f));
    Vec3f quatRotatedVec = myTransform.getRotation().rotateVector(Vec3f(0.0f, 0.0f, 1.0f));
    //std::cout << "Matrix:\n" << matrix << "\n\nOut vec: " << matRotatedVector << "\nRef vec: " << quatRotatedVec << std::endl;
    ASSERT_FLOAT_EQ(matRotatedVector.x, quatRotatedVec.x);
    ASSERT_FLOAT_EQ(matRotatedVector.y, quatRotatedVec.y);
    ASSERT_FLOAT_EQ(matRotatedVector.z, quatRotatedVec.z);
}

TEST(TransformTest, RotationMatrix2)
{
    TransformFloat myTransform;
    myTransform.rotateX(piF * 0.34f);
    myTransform.rotateZ(piF * 2.12f);
    myTransform.rotateY(piF * 1.23f);
    myTransform.rotate(Vec3f(0.3f, 0.5f, 1.2f).normalized(), 7.23f);

    Mat3x4f matrix = myTransform.getTransformMatrix();
    Vec3f matRotatedVector = matrix.transformVector(Vec3f(1.0f, 1.0f, 1.0f));
    Vec3f quatRotatedVec = myTransform.getRotation().rotateVector(Vec3f(1.0f, 1.0f, 1.0f));
    ASSERT_NEAR(matRotatedVector.x, quatRotatedVec.x, 10e-7f);
    ASSERT_NEAR(matRotatedVector.y, quatRotatedVec.y, 10e-7f);
    ASSERT_NEAR(matRotatedVector.z, quatRotatedVec.z, 10e-7f);
}

TEST(TransformTest, TranslationMatrix)
{
    TransformFloat myTransform;
    myTransform.translate(Vec3f(3.0f, 5.0f, 6.0f));
    myTransform.translateX(-2.0f); // 1  5  6
    myTransform.translateY(1.0f); //  1  6  6
    myTransform.translateZ(23.0f); // 1  6  29
    myTransform.translate(-Vec3f(4.0f, -3.0f, -10.0f)); // -3  9  39

    Mat3x4f matrix = myTransform.getTransformMatrix();
    Vec3f movedCenterpoint = matrix.transformPoint(Vec3f(0.0f));
    ASSERT_FLOAT_EQ(movedCenterpoint.x, -3.0f);
    ASSERT_FLOAT_EQ(movedCenterpoint.y, 9.0f);
    ASSERT_FLOAT_EQ(movedCenterpoint.z, 39.0f);

    Vec3f movedZeroVector = matrix.transformVector(Vec3f(0.0f));
    ASSERT_FLOAT_EQ(movedZeroVector.x, 0.0f);
    ASSERT_FLOAT_EQ(movedZeroVector.y, 0.0f);
    ASSERT_FLOAT_EQ(movedZeroVector.z, 0.0f);
}

TEST(TransformTest, TranslationScaleRotationMatrix)
{
    TransformFloat myTransform;
    myTransform.scale(Vec3f(2.0f, 0.5f, 1.0f));
    myTransform.rotateY(piF * 0.5f);
    myTransform.translate(Vec3f(2.0f, 1.0f, 3.0f));

    // Input vector is (1, 0, 0)
    // Scaled by (2, 0.5, 1):         (2, 0,  0)
    // Rotated around y axis by PI/2: (0, 0, -2)
    // Translated by (2, 1, 3):       (2, 1,  1)

    Mat3x4f matrix = myTransform.getTransformMatrix();
    Vec3f transformedPoint = matrix.transformPoint(Vec3f(1.0f, 0.0f, 0.0f));
    //std::cout << "Transformed point: " << transformedPoint << std::endl;
    ASSERT_FLOAT_EQ(transformedPoint.x, 2.0f);
    ASSERT_FLOAT_EQ(transformedPoint.y, 1.0f);
    ASSERT_FLOAT_EQ(transformedPoint.z, 1.0f);

    Vec3f transformedVector = matrix.transformVector(Vec3f(1.0, 0.0f, 0.0f));
    //std::cout << "Transformed vector: " << transformedVector << std::endl;
    ASSERT_FLOAT_EQ(transformedVector.x, 0.0f);
    ASSERT_FLOAT_EQ(transformedVector.y, 0.0f);
    ASSERT_FLOAT_EQ(transformedVector.z, -2.0f);
}
