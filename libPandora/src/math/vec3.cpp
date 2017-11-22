#include "pandora/math/vec3.h"
#include <type_traits>

namespace pandora
{

template<typename T>
Vec3<T>::Vec3()
{
	if constexpr (std::is_same<T, float>::value)
		x = y = z = 0.0f;
	else
		x = y = z = 0.0;
}

template<typename T>
Vec3<T>::Vec3(T value)
{
	x = y = z = value;
}

template<typename T>
Vec3<T>::Vec3(T x_, T y_, T z_)
{
	x = x_;
	y = y_;
	z = z_;
}

template<typename T>
Vec3<T>::Vec3(const Vec3<T>& other)
{
	x = other.x;
	y = other.y;
	z = other.z;
}


template<typename T>
Vec3<T> Vec3<T>::operator*(const Vec3<T>& other) const
{
	return Vec3<T>(x * other.x, y * other.y, z * other.z);
}

template<typename T>
Vec3<T> Vec3<T>::operator*(T amount) const
{
	return Vec3<T>(x * amount, y * amount, z * amount);
}

template<typename T>
Vec3<T> Vec3<T>::operator/(const Vec3<T>& other) const
{
	return Vec3<T>(x / other.x, y / other.y, z / other.z);
}

template<typename T>
Vec3<T> Vec3<T>::operator/(T amount) const
{
	return Vec3<T>(x / amount, y / amount, z / amount);
}

template<typename T>
Vec3<T> Vec3<T>::operator-(const Vec3<T>& other) const
{
	return Vec3<T>(x - other.x, y - other.y, z - other.z);
}

template<typename T>
Vec3<T> Vec3<T>::operator-(T amount) const
{
	return Vec3<T>(x - amount, y - amount, z - amount);
}

template<typename T>
Vec3<T> Vec3<T>::operator+(const Vec3<T>& other) const
{
	return Vec3<T>(x + other.x, y + other.y, z + other.z);
}

template<typename T>
Vec3<T> Vec3<T>::operator+(T amount) const
{
	return Vec3<T>(x + amount, y + amount, z + amount);
}

template<typename T>
Vec3<T> Vec3<T>::operator-() const
{
	return Vec3<T>(-x, -y, -z);
}

template<typename T>
void Vec3<T>::operator=(const Vec3<T>& other)
{
	x = other.x;
	y = other.y;
	z = other.z;
}

template<typename T>
void Vec3<T>::operator*=(const Vec3<T>& other)
{
	x *= other.x;
	y *= other.y;
	z *= other.z;
}

template<typename T>
void Vec3<T>::operator*=(T amount)
{
	x *= amount;
	y *= amount;
	z *= amount;
}

template<typename T>
void Vec3<T>::operator/=(const Vec3<T>& other)
{
	x /= other.x;
	y /= other.y;
	z /= other.z;
}

template<typename T>
void Vec3<T>::operator/=(T amount)
{
	x /= amount;
	y /= amount;
	z /= amount;
}

template<typename T>
void Vec3<T>::operator-=(const Vec3<T>& other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
}

template<typename T>
void Vec3<T>::operator-=(T amount)
{
	x -= amount;
	y -= amount;
	z -= amount;
}

template<typename T>
void Vec3<T>::operator+=(const Vec3<T>& other)
{
	x += other.x;
	y += other.y;
	z += other.z;
}

template<typename T>
void Vec3<T>::operator+=(T amount)
{
	x += amount;
	y += amount;
	z += amount;
}


template<typename T>
bool  Vec3<T>::operator==(const Vec3<T>& other) const
{
	return x == other.x && y == other.y && z == other.z;
}

template<typename T>
bool  Vec3<T>::operator!=(const Vec3<T>& other) const
{
	return x != other.x || y != other.y || z != other.z;
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

template float dot(const Vec3<float>& a, const Vec3<float>& b);
template double dot(const Vec3<double>& a, const Vec3<double>& b);
// Probably dont want to use dot product on int vectors?

template Vec3<float> cross(const Vec3<float>& a, const Vec3<float>& b);
template Vec3<double> cross(const Vec3<double>& a, const Vec3<double>& b);
// Probably dont want to use cross product on int vectors?

}
