#include "pandora/math/bounds3.h"
#include "pandora/math/constants.h"

namespace pandora {

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

template struct Bounds3<float>;
template struct Bounds3<double>;
}