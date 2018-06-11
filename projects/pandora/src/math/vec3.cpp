#include "pandora/math/vec3.h"
#include "pandora/math/constants.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <type_traits>

namespace pandora {

template <typename T>
Vec3<T>::Vec3()
{
    x = y = z = zero<T>();
}

template <typename T>
Vec3<T>::Vec3(T value)
{
    x = y = z = value;
}

template <typename T>
Vec3<T>::Vec3(T x_, T y_, T z_)
    : x(x_)
    , y(y_)
    , z(z_)
{
}

template <typename T>
Vec3<T>::Vec3(const Vec3<T>& other)
    : x(other.x)
    , y(other.y)
    , z(other.z)
{
}

template <typename T>
template <typename S>
Vec3<T>::Vec3(const Vec3<S>& other)
    : x(static_cast<T>(other.x))
    , y(static_cast<T>(other.y))
    , z(static_cast<T>(other.z))
{
}

template <typename T>
T Vec3<T>::length() const
{
    return std::sqrt(x * x + y * y + z * z);
}

template <>
int Vec3<int>::length() const
{
    assert(false); // Returning the length as an int would cause massive rounding errors
    return 0;
}

template <>
unsigned Vec3<unsigned>::length() const
{
    assert(false); // Returning the length as an int would cause massive rounding errors
    return 0;
}

template <typename T>
Vec3<T> Vec3<T>::normalized() const
{
    return *this / length();
}

template <>
Vec3<int> Vec3<int>::normalized() const
{
    assert(false); // Normalizing an int vector does not make much sense (rounding errors)
    return 0;
}

template <typename T>
T Vec3<T>::operator[](int i) const
{
    auto data = reinterpret_cast<const T*>(this);
    return data[i];
}

template <typename T>
T& Vec3<T>::operator[](int i)
{
    auto data = reinterpret_cast<T*>(this);
    return data[i];
}

template <typename T>
Vec3<T> Vec3<T>::operator*(const Vec3<T>& other) const
{
    return Vec3<T>(x * other.x, y * other.y, z * other.z);
}

template <typename T>
Vec3<T> Vec3<T>::operator*(T amount) const
{
    return Vec3<T>(x * amount, y * amount, z * amount);
}

template <typename T>
Vec3<T> Vec3<T>::operator/(const Vec3<T>& other) const
{
    return Vec3<T>(x / other.x, y / other.y, z / other.z);
}

template <typename T>
Vec3<T> Vec3<T>::operator/(T amount) const
{
    return Vec3<T>(x / amount, y / amount, z / amount);
}

template <typename T>
Vec3<T> Vec3<T>::operator-(const Vec3<T>& other) const
{
    return Vec3<T>(x - other.x, y - other.y, z - other.z);
}

template <typename T>
Vec3<T> Vec3<T>::operator-(T amount) const
{
    return Vec3<T>(x - amount, y - amount, z - amount);
}

template <typename T>
Vec3<T> Vec3<T>::operator+(const Vec3<T>& other) const
{
    return Vec3<T>(x + other.x, y + other.y, z + other.z);
}

template <typename T>
Vec3<T> Vec3<T>::operator+(T amount) const
{
    return Vec3<T>(x + amount, y + amount, z + amount);
}

template <typename T>
Vec3<T> operator*(T left, const Vec3<T>& right)
{
    return Vec3<T>(left * right.x, left * right.y, left * right.z);
}

template <typename T>
Vec3<T> operator/(T left, const Vec3<T>& right)
{
    return Vec3<T>(left / right.x, left / right.y, left / right.z);
}

template <typename T>
Vec3<T> operator-(T left, const Vec3<T>& right)
{
    return Vec3<T>(left - right.x, left - right.y, left - right.z);
}

template <typename T>
Vec3<T> operator+(T left, const Vec3<T>& right)
{
    return Vec3<T>(left + right.x, left + right.y, left + right.z);
}

template <typename T>
Vec3<T> Vec3<T>::operator-() const
{
    return Vec3<T>(-x, -y, -z);
}

template <>
Vec3<unsigned> Vec3<unsigned>::operator-() const
{
    assert(false);
    return Vec3<unsigned>(0u, 0u, 0u);
}

template <typename T>
void Vec3<T>::operator=(const Vec3<T>& other)
{
    x = other.x;
    y = other.y;
    z = other.z;
}

template <typename T>
void Vec3<T>::operator*=(const Vec3<T>& other)
{
    x *= other.x;
    y *= other.y;
    z *= other.z;
}

template <typename T>
void Vec3<T>::operator*=(T amount)
{
    x *= amount;
    y *= amount;
    z *= amount;
}

template <typename T>
void Vec3<T>::operator/=(const Vec3<T>& other)
{
    x /= other.x;
    y /= other.y;
    z /= other.z;
}

template <typename T>
void Vec3<T>::operator/=(T amount)
{
    x /= amount;
    y /= amount;
    z /= amount;
}

template <typename T>
void Vec3<T>::operator-=(const Vec3<T>& other)
{
    x -= other.x;
    y -= other.y;
    z -= other.z;
}

template <typename T>
void Vec3<T>::operator-=(T amount)
{
    x -= amount;
    y -= amount;
    z -= amount;
}

template <typename T>
void Vec3<T>::operator+=(const Vec3<T>& other)
{
    x += other.x;
    y += other.y;
    z += other.z;
}

template <typename T>
void Vec3<T>::operator+=(T amount)
{
    x += amount;
    y += amount;
    z += amount;
}

template <typename T>
bool Vec3<T>::operator==(const Vec3<T>& other) const
{
    return x == other.x && y == other.y && z == other.z;
}

template <typename T>
bool Vec3<T>::operator!=(const Vec3<T>& other) const
{
    return x != other.x || y != other.y || z != other.z;
}

template <typename T>
std::ostream& operator<<(std::ostream& stream, const Vec3<T>& vector)
{
    stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
    return stream;
}

template <typename T>
T dot(const Vec3<T>& a, const Vec3<T>& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b)
{
    return Vec3<T>(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

template <typename T>
Vec3<T> min(const Vec3<T>& a, const Vec3<T>& b)
{
    return Vec3<T>(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

template <typename T>
Vec3<T> max(const Vec3<T>& a, const Vec3<T>& b)
{
    return Vec3<T>(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

template <typename T>
Vec3<T> abs(const Vec3<T>& a)
{
    return Vec3<T>(std::abs(a.x), std::abs(a.y), std::abs(a.z));
}

template <typename T>
int maxDimension(const Vec3<T>& a)
{
    if (a.x > a.y) {
        if (a.x > a.z)
            return 0; // X
        else
            return 2; // Z
    } else {
        if (a.y > a.z)
            return 1; // Y
        else
            return 2; // Z
    }
}

template <typename T>
Vec3<T> permute(const Vec3<T>& a, int newX, int newY, int newZ)
{
    assert(newX >= 0 && newX < 3);
    assert(newY >= 0 && newY < 3);
    assert(newZ >= 0 && newZ < 3);

    const T* data = reinterpret_cast<const T*>(&a);
    return Vec3<T>(data[newX], data[newY], data[newZ]);
}

template Vec3<float>::Vec3(const Vec3<double>&);
template Vec3<double>::Vec3(const Vec3<float>&);

template class Vec3<float>;
template class Vec3<double>;
template class Vec3<int>;
template class Vec3<unsigned>;

template Vec3<float> operator*(float, const Vec3<float>&);
template Vec3<double> operator*(double, const Vec3<double>&);
template Vec3<int> operator*(int, const Vec3<int>&);

template Vec3<float> operator/(float, const Vec3<float>&);
template Vec3<double> operator/(double, const Vec3<double>&);
template Vec3<int> operator/(int, const Vec3<int>&);

template Vec3<float> operator-(float, const Vec3<float>&);
template Vec3<double> operator-(double, const Vec3<double>&);
template Vec3<int> operator-(int, const Vec3<int>&);

template Vec3<float> operator+(float, const Vec3<float>&);
template Vec3<double> operator+(double, const Vec3<double>&);
template Vec3<int> operator+(int, const Vec3<int>&);

template std::ostream& operator<<(std::ostream&, const Vec3<float>&);
template std::ostream& operator<<(std::ostream&, const Vec3<double>&);
template std::ostream& operator<<(std::ostream&, const Vec3<int>&);

template float dot(const Vec3<float>&, const Vec3<float>&);
template double dot(const Vec3<double>&, const Vec3<double>&);
// Probably dont want to use dot product on int vectors?

template Vec3<float> cross(const Vec3<float>&, const Vec3<float>&);
template Vec3<double> cross(const Vec3<double>&, const Vec3<double>&);
// Probably dont want to use cross product on int vectors?

template Vec3<float> min(const Vec3<float>&, const Vec3<float>&);
template Vec3<double> min(const Vec3<double>&, const Vec3<double>&);
template Vec3<int> min(const Vec3<int>&, const Vec3<int>&);

template Vec3<float> max(const Vec3<float>&, const Vec3<float>&);
template Vec3<double> max(const Vec3<double>&, const Vec3<double>&);
template Vec3<int> max(const Vec3<int>&, const Vec3<int>&);

template Vec3<float> abs(const Vec3<float>&);
template Vec3<double> abs(const Vec3<double>&);
template Vec3<int> abs(const Vec3<int>&);

template int maxDimension(const Vec3<float>&);
template int maxDimension(const Vec3<double>&);
template int maxDimension(const Vec3<int>&);

template Vec3<float> permute(const Vec3<float>&, int, int, int);
template Vec3<double> permute(const Vec3<double>&, int, int, int);
template Vec3<int> permute(const Vec3<int>&, int, int, int);
}
