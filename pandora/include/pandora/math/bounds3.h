#pragma once
#include "pandora/math/vec3.h"

namespace pandora {
template <typename T>
struct Bounds3 {
public:
    void reset();
    void grow(Vec3<T> vec);

public:
    Vec3<T> bounds_min, bounds_max;
};

using Bounds3f = Bounds3<float>;
using Bounds3d = Bounds3<double>;
}