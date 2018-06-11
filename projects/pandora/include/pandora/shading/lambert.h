#pragma once
#include "math/vec3.h"

class LambertBxDF
{
    glm::vec3 sampleF(const glm::vec3& wo, glm::vec3& wi, const Point2f& sample, float& pdf, BxDFType* sampledType = nullptr) const = 0;
};