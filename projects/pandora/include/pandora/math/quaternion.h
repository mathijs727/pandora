#pragma once
#include "pandora/math/mat3x4.h"
#include "pandora/math/vec3.h"

namespace pandora {

template <typename T>
class UnitQuaternion {
public:
    UnitQuaternion();

    static UnitQuaternion<T> rotation(const Vec3<T>& vector, T angle);
    static UnitQuaternion<T> eulerAngles(const Vec3<T>& angles);

    Vec3<T> rotateVector(const Vec3<T>& vector) const;
    Mat3x4<T> rotationMatrix() const;

    UnitQuaternion<T> operator*(const UnitQuaternion<T>& other) const;

private:
    UnitQuaternion(T scalar, const Vec3<T>& vector);

private:
    UnitQuaternion<T> conjugate() const;
    UnitQuaternion<T> inverse() const;

private:
    T m_r;
    Vec3<T> m_v;
};

using QuatF = UnitQuaternion<float>;
using QuatD = UnitQuaternion<double>;
}