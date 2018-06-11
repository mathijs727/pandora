#pragma once
#include "math/vec3.h"

class LambertBxDF
{
    Vec3f sampleF(const Vec3f& wo, Vec3f& wi, const Point2f& sample, float& pdf, BxDFType* sampledType = nullptr) const = 0;
};