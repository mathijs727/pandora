#pragma once
#include "pandora/math/vec3.h"
#include "pandora/traversal/ray.h"

namespace pandora {
template <typename T>
struct Bounds3 {
public:
    Bounds3();
    Bounds3(Vec3<T> lower, Vec3<T> upper);

    void reset();
    void grow(Vec3<T> vec);
    void extend(const Bounds3<T>& other);
    Bounds3 extended(const Bounds3<T>& other) const;

    Vec3<T> center() const;
    T area() const;
    T halfArea() const;

    bool intersect(const Ray& ray, T& tmin, T& tmax);

public:
    Vec3<T> bounds_min, bounds_max;
};

using Bounds3f = Bounds3<float>;
using Bounds3d = Bounds3<double>;
}