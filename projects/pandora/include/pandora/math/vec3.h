#pragma once
#include <ostream>

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

    template <typename S>
    Vec3(const Vec3<S>& other);

    T length() const;
    Vec3<T> normalized() const;

    T operator[](int i) const;
    T& operator[](int i);

    // Binary operators
    Vec3<T> operator*(const Vec3<T>& other) const;
    Vec3<T> operator*(T amount) const;
    Vec3<T> operator/(const Vec3<T>& other) const;
    Vec3<T> operator/(T amount) const;
    Vec3<T> operator-(const Vec3<T>& other) const;
    Vec3<T> operator-(T amount) const;
    Vec3<T> operator+(const Vec3<T>& other) const;
    Vec3<T> operator+(T amount) const;

    template <typename S>
    friend Vec3<S> operator*(S, const Vec3<S>&);

    template <typename S>
    friend Vec3<S> operator/(S, const Vec3<S>&);

    template <typename S>
    friend Vec3<S> operator-(S, const Vec3<S>&);

    template <typename S>
    friend Vec3<S> operator+(S, const Vec3<S>&);

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

    // Output operator
    template <typename S>
    friend std::ostream& operator<<(std::ostream&, const Vec3<S>&);
};

template <typename T>
T dot(const Vec3<T>& a, const Vec3<T>& b);

template <typename T>
Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b);

template <typename T>
Vec3<T> min(const Vec3<T>& a, const Vec3<T>& b);

template <typename T>
Vec3<T> max(const Vec3<T>& a, const Vec3<T>& b);

template <typename T>
Vec3<T> abs(const Vec3<T>& a);

template <typename T>
int maxDimension(const Vec3<T>& a);

template <typename T>
Vec3<T> permute(const Vec3<T>& a, int newX, int newY, int newZ);

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int>;
using Vec3u = Vec3<unsigned>;

using Point3f = Vec3<float>;
using Normal3f = Vec3<float>;

}
