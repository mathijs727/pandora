#include "pandora/math/vec3.h"
#include "pandora/math/constants.h"
#include <cmath>
#include <type_traits>
#include <cassert>

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
T Vec3<T>::length() const
{
	if constexpr (std::is_same<T, int>::value)
	{
		assert(false);// Dont call length() on an int vector
		return 0;
	} else {
		return std::sqrt(x * x + y * y + z * z);
	}
}

template <typename T>
Vec3<T> Vec3<T>::normalized() const
{
	if constexpr (std::is_same<T, int>::value)
	{
		assert(false);// Dont call normalized() on an int vector
		return 0;
	} else {
		return *this / length();
	}
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

template class Vec3<float>;
template class Vec3<double>;
template class Vec3<int>;

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
}
