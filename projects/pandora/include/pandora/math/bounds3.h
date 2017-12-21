#pragma once
#include "pandora/math/vec3.h"
#include "pandora/traversal/ray.h"

namespace pandora {
template <typename T>
struct Bounds3 {
public:
    void reset();
    void grow(Vec3<T> vec);
    void merge(const Bounds3<T>& other);

    bool intersect(const Ray& ray, T& tmin, T& tmax);

public:
    Vec3<T> bounds_min, bounds_max;
};

using Bounds3f = Bounds3<float>;
using Bounds3d = Bounds3<double>;
}