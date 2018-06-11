#include "pandora/math/constants.h"
#include "pandora/math/mat3x4.h"
#include "glm/gtc/quaternion.hpp"
#include "pandora/math/transform.h"
#include "glm/glm.hpp"
#include "gtest/gtest.h"
#include <iostream>

using namespace pandora;

TEST(TransformTest, Quaternion1)
{
    glm::vec3 xAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 yAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 zAxis = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::quat rotation1 = glm::quat::rotation(xAxis, piF); //          0,  0, -1
    glm::quat rotation2 = glm::quat::rotation(-yAxis, piF / 2.0f); //  1,  0,  0
    glm::quat finalRotation = rotation2 * rotation1;

    // ASSERT_FLOAT_EQ does not except the rounding errors
    glm::vec3 rotatedVector = finalRotation.rotateVector(zAxis);
    ASSERT_NEAR(rotatedVector.x, 1.0f, 10e-7f);
    ASSERT_NEAR(rotatedVector.y, 0.0f, 10e-7f);
    ASSERT_NEAR(rotatedVector.z, 0.0f, 10e-7f);
}

TEST(TransformTest, Quaternion2)
{
    glm::vec3 xAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 yAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 zAxis = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::quat rotation1 = glm::quat::rotation(zAxis, piF * 1.5f); //  0, -1,  0
    glm::quat rotation2 = glm::quat::rotation(xAxis, piF * 0.5f); //  0,  0, -1
    glm::quat rotation3 = glm::quat::rotation(yAxis, piF * 1.5f); //  1,  0,  0
    glm::quat finalRotation = rotation3 * rotation2 * rotation1;

    glm::vec3 rotatedVector = finalRotation.rotateVector(xAxis);
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
    glm::vec3 matRotatedVector = matrix.transformVector(glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 quatRotatedVec = myTransform.getRotation().rotateVector(glm::vec3(0.0f, 0.0f, 1.0f));
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
    myTransform.rotate(glm::vec3(0.3f, 0.5f, 1.2f).normalized(), 7.23f);

    Mat3x4f matrix = myTransform.getTransformMatrix();
    glm::vec3 matRotatedVector = matrix.transformVector(glm::vec3(1.0f, 1.0f, 1.0f));
    glm::vec3 quatRotatedVec = myTransform.getRotation().rotateVector(glm::vec3(1.0f, 1.0f, 1.0f));
    ASSERT_NEAR(matRotatedVector.x, quatRotatedVec.x, 10e-7f);
    ASSERT_NEAR(matRotatedVector.y, quatRotatedVec.y, 10e-7f);
    ASSERT_NEAR(matRotatedVector.z, quatRotatedVec.z, 10e-7f);
}

TEST(TransformTest, TranslationMatrix)
{
    TransformFloat myTransform;
    myTransform.translate(glm::vec3(3.0f, 5.0f, 6.0f));
    myTransform.translateX(-2.0f); // 1  5  6
    myTransform.translateY(1.0f); //  1  6  6
    myTransform.translateZ(23.0f); // 1  6  29
    myTransform.translate(-glm::vec3(4.0f, -3.0f, -10.0f)); // -3  9  39

    Mat3x4f matrix = myTransform.getTransformMatrix();
    glm::vec3 movedCenterpoint = matrix.transformPoint(glm::vec3(0.0f));
    ASSERT_FLOAT_EQ(movedCenterpoint.x, -3.0f);
    ASSERT_FLOAT_EQ(movedCenterpoint.y, 9.0f);
    ASSERT_FLOAT_EQ(movedCenterpoint.z, 39.0f);

    glm::vec3 movedZeroVector = matrix.transformVector(glm::vec3(0.0f));
    ASSERT_FLOAT_EQ(movedZeroVector.x, 0.0f);
    ASSERT_FLOAT_EQ(movedZeroVector.y, 0.0f);
    ASSERT_FLOAT_EQ(movedZeroVector.z, 0.0f);
}

TEST(TransformTest, TranslationScaleRotationMatrix)
{
    TransformFloat myTransform;
    myTransform.scale(glm::vec3(2.0f, 0.5f, 1.0f));
    myTransform.rotateY(piF * 0.5f);
    myTransform.translate(glm::vec3(2.0f, 1.0f, 3.0f));

    // Input vector is (1, 0, 0)
    // Scaled by (2, 0.5, 1):         (2, 0,  0)
    // Rotated around y axis by PI/2: (0, 0, -2)
    // Translated by (2, 1, 3):       (2, 1,  1)

    Mat3x4f matrix = myTransform.getTransformMatrix();
    glm::vec3 transformedPoint = matrix.transformPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    //std::cout << "Transformed point: " << transformedPoint << std::endl;
    ASSERT_FLOAT_EQ(transformedPoint.x, 2.0f);
    ASSERT_FLOAT_EQ(transformedPoint.y, 1.0f);
    ASSERT_FLOAT_EQ(transformedPoint.z, 1.0f);

    glm::vec3 transformedVector = matrix.transformVector(glm::vec3(1.0, 0.0f, 0.0f));
    //std::cout << "Transformed vector: " << transformedVector << std::endl;
    ASSERT_FLOAT_EQ(transformedVector.x, 0.0f);
    ASSERT_FLOAT_EQ(transformedVector.y, 0.0f);
    ASSERT_FLOAT_EQ(transformedVector.z, -2.0f);
}
