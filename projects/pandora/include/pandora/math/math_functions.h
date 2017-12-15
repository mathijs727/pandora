#pragma once
#include "pandora/math/constants.h"

namespace pandora {

float radianToDegrees(float radian)
{
    return radian / piF * 180.0f;
}

float degreesToRadian(float degrees)
{
    return degrees / 180.0f * piF;
}
}