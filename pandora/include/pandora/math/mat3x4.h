#pragma once
#include "pandora/math/vec3.h"
#include <array>
#include <ostream>
#include <tuple>

namespace pandora {

template <typename T>
class UnitQuaternion;

template <typename T>
class Transform;

template <typename T>
class Mat3x4 {
public:
    Mat3x4() = delete;

    static Mat3x4<T> translation(const Vec3<T>& offset);
    static Mat3x4<T> scale(const Vec3<T>& amount);

    T operator[](const std::pair<int, int>& index) const;
    Vec3<T> transformPoint(const Vec3<T>& point) const;
    Vec3<T> transformVector(const Vec3<T>& vector) const;

    Mat3x4<T> operator*(const Mat3x4<T>& right) const;

    template <typename S>
    friend std::ostream& operator<<(std::ostream&, const Mat3x4<S>&);

private:
    friend class UnitQuaternion<T>;

    Mat3x4(const T values[3][4]);

private:
    T m_values[3][4];
};

using Mat3x4f = Mat3x4<float>;
using Mat3x4d = Mat3x4<double>;
}