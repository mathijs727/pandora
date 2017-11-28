#include "pandora/math/vec2.h"
#include "pandora/math/constants.h"
#include <cassert>
#include <cmath>
#include <type_traits>

namespace pandora {

template <typename T>
Vec2<T>::Vec2()
{
    x = y = zero<T>();
}

template <typename T>
Vec2<T>::Vec2(T value)
{
    x = y = value;
}

template <typename T>
Vec2<T>::Vec2(T x_, T y_)
    : x(x_)
    , y(y_)
{
}

template <typename T>
Vec2<T>::Vec2(const Vec2<T>& other)
    : x(other.x)
    , y(other.y)
{
}

template <typename T>
template <typename S>
Vec2<T>::operator Vec2<S>() const
{
    return Vec2<S>(static_cast<S>(x), static_cast<S>(y));
}

template <typename T>
T Vec2<T>::length() const
{
    return std::sqrt(x * x + y * y);
}

template <>
int Vec2<int>::length() const
{
    assert(false); // Returning the length as an int would cause massive rounding errors
    return 0;
}

template <typename T>
Vec2<T> Vec2<T>::normalized() const
{
    return *this / length();
}

template <>
Vec2<int> Vec2<int>::normalized() const
{
    assert(false); // Normalizing an int vector does not make much sense (rounding errors)
    return 0;
}

template <typename T>
Vec2<T> Vec2<T>::operator*(const Vec2<T>& other) const
{
    return Vec2<T>(x * other.x, y * other.y);
}

template <typename T>
Vec2<T> Vec2<T>::operator*(T amount) const
{
    return Vec2<T>(x * amount, y * amount);
}

template <typename T>
Vec2<T> Vec2<T>::operator/(const Vec2<T>& other) const
{
    return Vec2<T>(x / other.x, y / other.y);
}

template <typename T>
Vec2<T> Vec2<T>::operator/(T amount) const
{
    return Vec2<T>(x / amount, y / amount);
}

template <typename T>
Vec2<T> Vec2<T>::operator-(const Vec2<T>& other) const
{
    return Vec2<T>(x - other.x, y - other.y);
}

template <typename T>
Vec2<T> Vec2<T>::operator-(T amount) const
{
    return Vec2<T>(x - amount, y - amount);
}

template <typename T>
Vec2<T> Vec2<T>::operator+(const Vec2<T>& other) const
{
    return Vec2<T>(x + other.x, y + other.y);
}

template <typename T>
Vec2<T> Vec2<T>::operator+(T amount) const
{
    return Vec2<T>(x + amount, y + amount);
}

template <typename T>
Vec2<T> operator*(T left, const Vec2<T>& right)
{
    return Vec2<T>(left * right.x, left * right.y);
}

template <typename T>
Vec2<T> operator/(T left, const Vec2<T>& right)
{
    return Vec2<T>(left / right.x, left / right.y);
}

template <typename T>
Vec2<T> operator-(T left, const Vec2<T>& right)
{
    return Vec2<T>(left - right.x, left - right.y);
}

template <typename T>
Vec2<T> operator+(T left, const Vec2<T>& right)
{
    return Vec2<T>(left + right.x, left + right.y);
}

template <typename T>
Vec2<T> Vec2<T>::operator-() const
{
    return Vec2<T>(-x, -y);
}

template <typename T>
void Vec2<T>::operator=(const Vec2<T>& other)
{
    x = other.x;
    y = other.y;
}

template <typename T>
void Vec2<T>::operator*=(const Vec2<T>& other)
{
    x *= other.x;
    y *= other.y;
}

template <typename T>
void Vec2<T>::operator*=(T amount)
{
    x *= amount;
    y *= amount;
}

template <typename T>
void Vec2<T>::operator/=(const Vec2<T>& other)
{
    x /= other.x;
    y /= other.y;
}

template <typename T>
void Vec2<T>::operator/=(T amount)
{
    x /= amount;
    y /= amount;
}

template <typename T>
void Vec2<T>::operator-=(const Vec2<T>& other)
{
    x -= other.x;
    y -= other.y;
}

template <typename T>
void Vec2<T>::operator-=(T amount)
{
    x -= amount;
    y -= amount;
}

template <typename T>
void Vec2<T>::operator+=(const Vec2<T>& other)
{
    x += other.x;
    y += other.y;
}

template <typename T>
void Vec2<T>::operator+=(T amount)
{
    x += amount;
    y += amount;
}

template <typename T>
bool Vec2<T>::operator==(const Vec2<T>& other) const
{
    return x == other.x && y == other.y;
}

template <typename T>
bool Vec2<T>::operator!=(const Vec2<T>& other) const
{
    return x != other.x || y != other.y;
}

template <typename T>
std::ostream& operator<<(std::ostream& stream, const Vec2<T>& vector)
{
    stream << "(" << vector.x << ", " << vector.y << ")";
    return stream;
}

template class Vec2<float>;
template class Vec2<double>;
template class Vec2<int>;

template Vec2<int>::operator Vec2<float>() const;
template Vec2<int>::operator Vec2<double>() const;
template Vec2<float>::operator Vec2<int>() const;
template Vec2<float>::operator Vec2<double>() const;
template Vec2<double>::operator Vec2<int>() const;
template Vec2<double>::operator Vec2<float>() const;

template Vec2<float> operator*(float, const Vec2<float>&);
template Vec2<double> operator*(double, const Vec2<double>&);
template Vec2<int> operator*(int, const Vec2<int>&);

template Vec2<float> operator/(float, const Vec2<float>&);
template Vec2<double> operator/(double, const Vec2<double>&);
template Vec2<int> operator/(int, const Vec2<int>&);

template Vec2<float> operator-(float, const Vec2<float>&);
template Vec2<double> operator-(double, const Vec2<double>&);
template Vec2<int> operator-(int, const Vec2<int>&);

template Vec2<float> operator+(float, const Vec2<float>&);
template Vec2<double> operator+(double, const Vec2<double>&);
template Vec2<int> operator+(int, const Vec2<int>&);

template std::ostream& operator<<(std::ostream&, const Vec2<float>&);
template std::ostream& operator<<(std::ostream&, const Vec2<double>&);
template std::ostream& operator<<(std::ostream&, const Vec2<int>&);
}
