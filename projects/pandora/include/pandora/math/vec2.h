#pragma once
#include <ostream>

namespace pandora {

template <typename T>
class Vec2 {
public:
    T x, y;

public:
    Vec2();
    Vec2(T value);
    Vec2(T x, T y);
    Vec2(const Vec2<T>& other);

    template <typename S>
    operator Vec2<S>() const;

    T length() const;
    Vec2<T> normalized() const;

    // Binary operators
    Vec2<T> operator*(const Vec2<T>& other) const;
    Vec2<T> operator*(T amount) const;
    Vec2<T> operator/(const Vec2<T>& other) const;
    Vec2<T> operator/(T amount) const;
    Vec2<T> operator-(const Vec2<T>& other) const;
    Vec2<T> operator-(T amount) const;
    Vec2<T> operator+(const Vec2<T>& other) const;
    Vec2<T> operator+(T amount) const;

    template <typename S>
    friend Vec2<S> operator*(S, const Vec2<S>&);

    template <typename S>
    friend Vec2<S> operator/(S, const Vec2<S>&);

    template <typename S>
    friend Vec2<S> operator-(S, const Vec2<S>&);

    template <typename S>
    friend Vec2<S> operator+(S, const Vec2<S>&);

    // Unary operators
    Vec2<T> operator-() const;

    // Assignment operators
    void operator=(const Vec2<T>& other);
    void operator*=(const Vec2<T>& other);
    void operator*=(T amount);
    void operator/=(const Vec2<T>& other);
    void operator/=(T amount);
    void operator-=(const Vec2<T>& other);
    void operator-=(T amount);
    void operator+=(const Vec2<T>& other);
    void operator+=(T amount);

    // Comparison operators
    bool operator==(const Vec2<T>& other) const;
    bool operator!=(const Vec2<T>& other) const;

    // Output operator
    template <typename S>
    friend std::ostream& operator<<(std::ostream&, const Vec2<S>&);
};

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int>;

template <typename T>
template <typename S>
inline Vec2<T>::operator Vec2<S>() const
{
    return Vec2<S>(static_cast<S>(x), static_cast<S>(y));
}
}
