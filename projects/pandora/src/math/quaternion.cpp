#include "pandora/math/quaternion.h"
#include "pandora/math/constants.h"
#include <cmath>

namespace pandora {

template <typename T>
UnitQuaternion<T>::UnitQuaternion()
    : m_r(one<T>())
    , m_v(zero<T>())
{
}

template <typename T>
UnitQuaternion<T>::UnitQuaternion(T scalar, const Vec3<T>& vector)
    : m_r(scalar)
    , m_v(vector)
{
}

template <typename T>
UnitQuaternion<T> UnitQuaternion<T>::rotation(const Vec3<T>& axis, T angle)
{
    return UnitQuaternion<T>(std::cos(angle / 2), axis.normalized() * std::sin(angle / 2));
}

template <typename T>
UnitQuaternion<T> UnitQuaternion<T>::eulerAngles(const Vec3<T>& angles)
{
    T pitch = angles.x;
    T yaw = angles.y;
    T roll = angles.z;

    auto pitchRotation = UnitQuaternion<T>::rotation(Vec3<T>(1, 0, 0), pitch);
    auto yawRotation = UnitQuaternion<T>::rotation(Vec3<T>(0, 1, 0), yaw);
    auto rollRotation = UnitQuaternion<T>::rotation(Vec3<T>(0, 0, 1), roll);
    return rollRotation * yawRotation * pitchRotation;

    // https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
    /*T cy = std::cos(yaw * 0.5);
    T sy = std::sin(yaw * 0.5);
    T cr = std::cos(roll * 0.5);
    T sr = std::sin(roll * 0.5);
    T cp = std::cos(pitch * 0.5);
    T sp = std::sin(pitch * 0.5);

    T w = cy * cr * cp + sy * sr * sp;
    T x = cy * sr * cp - sy * cr * sp;
    T y = cy * cr * sp + sy * sr * cp;
    T z = sy * cr * cp - cy * sr * sp;

    return UnitQuaternion<T>(w, Vec3<T>(x, y, z));*/
}

template <typename T>
Vec3<T> UnitQuaternion<T>::rotateVector(const Vec3<T>& vector) const
{
    T two = std::is_same<T, float>::value ? 2.0f : 2.0;

    // https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
    return two * dot(m_v, vector) * m_v
        + (m_r * m_r - dot(m_v, m_v)) * vector
        + two * m_r * cross(m_v, vector);
}

template <typename T>
Mat3x4<T> UnitQuaternion<T>::rotationMatrix() const
{
    T a = m_r;
    T b = m_v.x;
    T c = m_v.y;
    T d = m_v.z;

    // http://run.usc.edu/cs520-s12/quaternions/quaternions-cs520.pdf
    T values[3][4];
    /*values[0][0] = a * a + b * b - c * c - d * d;
    values[0][1] = 2 * b * c + 2 * a * d;
    values[0][2] = 2 * b * d - 2 * a * c;

    values[1][0] = 2 * b * c - 2 * a * d;
    values[1][1] = a * a - b * b + c * c - d * d;
    values[1][2] = 2 * c * d + 2 * a * b;

    values[2][0] = 2 * b * d + 2 * a * c;
    values[2][1] = 2 * c * d - 2 * a * b;
    values[2][2] = a * a - b * b - c * c + d * d;*/

    values[0][0] = a * a + b * b - c * c - d * d;
    values[0][1] = 2 * b * c - 2 * a * d;
    values[0][2] = 2 * b * d + 2 * a * c;
    values[0][3] = zero<T>();

    values[1][0] = 2 * b * c + 2 * a * d;
    values[1][1] = a * a - b * b + c * c - d * d;
    values[1][2] = 2 * c * d - 2 * a * b;
    values[1][3] = zero<T>();

    values[2][0] = 2 * b * d - 2 * a * c;
    values[2][1] = 2 * c * d + 2 * a * b;
    values[2][2] = a * a - b * b - c * c + d * d;
    values[2][3] = zero<T>();

    return Mat3x4<T>(values);
}

template <typename T>
UnitQuaternion<T> UnitQuaternion<T>::operator*(const UnitQuaternion<T>& other) const
{
    T scalar = m_r * other.m_r - dot(m_v, other.m_v);
    Vec3<T> vector = m_r * other.m_v + other.m_r * m_v + cross(m_v, other.m_v);

    // Normalize to prevent floating point drift
    T len = std::sqrt(scalar * scalar + dot(vector, vector));
    scalar /= len;
    vector /= len;

    return UnitQuaternion<T>(scalar, vector);
}

template <typename T>
UnitQuaternion<T> UnitQuaternion<T>::conjugate() const
{
    return UnitQuaternion<T>(m_r, -m_v);
}

template <typename T>
UnitQuaternion<T> UnitQuaternion<T>::inverse() const
{
    return conjugate();
}

template class UnitQuaternion<float>;
template class UnitQuaternion<double>;
}
