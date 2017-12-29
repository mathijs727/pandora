#include "pandora/math/bounds3.h"
#include "pandora/math/constants.h"
#include <algorithm>

namespace pandora {

template <typename T>
Bounds3<T>::Bounds3()
    : bounds_min(zero<T>())
    , bounds_max(zero<T>())
{
}

template <typename T>
Bounds3<T>::Bounds3(Vec3<T> lower, Vec3<T> upper)
    : bounds_min(lower)
    , bounds_max(upper)
{
}

template <typename T>
void Bounds3<T>::reset()
{
    bounds_min = zero<T>();
    bounds_max = zero<T>();
}

template <typename T>
void Bounds3<T>::grow(Vec3<T> vec)
{
    bounds_min = min(bounds_min, vec);
    bounds_max = max(bounds_max, vec);
}

template <typename T>
void Bounds3<T>::extend(const Bounds3<T>& other)
{
    bounds_min = min(bounds_min, other.bounds_min);
    bounds_max = max(bounds_max, other.bounds_max);
}

template <typename T>
Bounds3<T> Bounds3<T>::extended(const Bounds3<T>& other) const
{
    auto minBounds = min(bounds_min, other.bounds_min);
    auto maxBounds = max(bounds_max, other.bounds_max);
    return { minBounds, maxBounds };
}

template <typename T>
Vec3<T> Bounds3<T>::center() const
{
    return (bounds_min + bounds_max) / 2.0f;
}

template <typename T>
T Bounds3<T>::area() const
{
    return 2 * halfArea();
}

template <typename T>
T Bounds3<T>::halfArea() const
{
    Vec3<T> extent = bounds_max - bounds_min;
    return extent.x * extent.y * extent.z;
}

template <typename T>
bool Bounds3<T>::intersect(const Ray& ray, T& tmin, T& tmax)
{
    tmin = std::numeric_limits<T>::lowest();
    tmax = std::numeric_limits<T>::max();

    if (ray.direction.x != 0.0f) {
        T tx1 = (bounds_min.x - ray.origin.x) / ray.direction.x;
        T tx2 = (bounds_max.x - ray.origin.x) / ray.direction.x;

        tmin = std::max(tmin, std::min(tx1, tx2));
        tmax = std::min(tmax, std::max(tx1, tx2));
    }

    if (ray.direction.y != 0.0f) {
        T ty1 = (bounds_min.y - ray.origin.y) / ray.direction.y;
        T ty2 = (bounds_max.y - ray.origin.y) / ray.direction.y;

        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));
    }

    if (ray.direction.z != 0.0f) {
        T tz1 = (bounds_min.z - ray.origin.z) / ray.direction.z;
        T tz2 = (bounds_max.z - ray.origin.z) / ray.direction.z;

        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));
    }

    // tmax >= 0: prevent boxes before the starting position from being hit
    // See the comment section at:
    //  https://tavianator.com/fast-branchless-raybounding-box-intersections/
    return tmax >= tmin && tmax >= zero<T>();
}

template struct Bounds3<float>;
template struct Bounds3<double>;
}