#pragma once

namespace pandora {


template <typename T>
class Vec3 {
public:
    T x, y, z;
public:
    Vec3();
    Vec3(T value);
    Vec3(T x, T y, T z);
    Vec3(const Vec3<T>& other);

    // Binary operators
    Vec3<T> operator*(const Vec3<T>& other) const;
    Vec3<T> operator*(T amount) const;
    Vec3<T> operator/(const Vec3<T>& other) const;
    Vec3<T> operator/(T amount) const;
    Vec3<T> operator-(const Vec3<T>& other) const;
    Vec3<T> operator-(T amount) const;
    Vec3<T> operator+(const Vec3<T>& other) const;
    Vec3<T> operator+(T amount) const;

    // Unary operators
    Vec3<T> operator-() const;

    // Assignment operators
    void operator=(const Vec3<T>& other);
    void operator*=(const Vec3<T>& other);
    void operator*=(T amount);
    void operator/=(const Vec3<T>& other);
    void operator/=(T amount);
    void operator-=(const Vec3<T>& other);
    void operator-=(T amount);
    void operator+=(const Vec3<T>& other);
    void operator+=(T amount);

    // Comparison operators
    bool operator==(const Vec3<T>& other) const;
    bool operator!=(const Vec3<T>& other) const;
};

template <typename T>
T dot(const Vec3<T>& a, const Vec3<T>& b);

template <typename T>
Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b);

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int>;
}