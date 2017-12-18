#include "pandora/math/mat3x4.h"
#include "pandora/math/constants.h"
#include <algorithm>
#include <cstring>

namespace pandora {

template <typename T>
Mat3x4<T>::Mat3x4()
{
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 4; col++)
            m_values[row][col] = zero<T>();
}

template <typename T>
Mat3x4<T>::Mat3x4(const T values[3][4])
{
    std::memcpy(m_values, values, sizeof(T) * 3 * 4);
}

template <typename T>
Mat3x4<T> Mat3x4<T>::identity()
{
    Mat3x4 result;
    result.m_values[0][0] = one<T>();
    result.m_values[1][1] = one<T>();
    result.m_values[2][2] = one<T>();
    return result;
}

template <typename T>
Mat3x4<T> Mat3x4<T>::translation(const Vec3<T>& offset)
{
    Mat3x4 result;
    result.m_values[0][0] = one<T>();
    result.m_values[1][1] = one<T>();
    result.m_values[2][2] = one<T>();
    result.m_values[0][3] = offset.x;
    result.m_values[1][3] = offset.y;
    result.m_values[2][3] = offset.z;
    return result;
}

template <typename T>
Mat3x4<T> Mat3x4<T>::scale(const Vec3<T>& amount)
{
    Mat3x4 result;
    result.m_values[0][0] = amount.x;
    result.m_values[1][1] = amount.y;
    result.m_values[2][2] = amount.z;
    return result;
}

template <typename T>
T Mat3x4<T>::operator[](const std::pair<int, int>& index) const
{
    return m_values[index.first][index.second];
}

template <typename T>
Vec3<T> Mat3x4<T>::transformPoint(const Vec3<T>& point) const
{
    T x = point.x * m_values[0][0] + point.y * m_values[0][1] + point.z * m_values[0][2] + m_values[0][3];
    T y = point.x * m_values[1][0] + point.y * m_values[1][1] + point.z * m_values[1][2] + m_values[1][3];
    T z = point.x * m_values[2][0] + point.y * m_values[2][1] + point.z * m_values[2][2] + m_values[2][3];
    return Vec3<T>(x, y, z);
}

template <typename T>
Vec3<T> Mat3x4<T>::transformVector(const Vec3<T>& vector) const
{
    T x = vector.x * m_values[0][0] + vector.y * m_values[0][1] + vector.z * m_values[0][2];
    T y = vector.x * m_values[1][0] + vector.y * m_values[1][1] + vector.z * m_values[1][2];
    T z = vector.x * m_values[2][0] + vector.y * m_values[2][1] + vector.z * m_values[2][2];
    return Vec3<T>(x, y, z);
}

template <typename T>
Mat3x4<T> Mat3x4<T>::operator*(const Mat3x4<T>& right) const
{
    T values[3][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            values[j][i] = m_values[j][0] * right.m_values[0][i] + m_values[j][1] * right.m_values[1][i] + m_values[j][2] * right.m_values[2][i] + m_values[j][3] * (i < 3 ? zero<T>() : one<T>());
        }
    }
    return Mat3x4<T>(values);
}

template <typename T>
Mat3x4<T> Mat3x4<T>::operator*=(const Mat3x4<T>& right) const
{
    return (*this) * right;
}

template <typename T>
std::ostream& operator<<(std::ostream& stream, const Mat3x4<T>& matrix)
{
    auto values = matrix.m_values;
    stream << "[[ " << values[0][0] << ", " << values[0][1] << ", " << values[0][2] << ", " << values[0][3] << " ]\n";
    stream << " [ " << values[1][0] << ", " << values[1][1] << ", " << values[1][2] << ", " << values[1][3] << " ]\n";
    stream << " [ " << values[2][0] << ", " << values[2][1] << ", " << values[2][2] << ", " << values[2][3] << " ]\n";
    stream << " [ " << zero<T>() << ", " << zero<T>() << ", " << zero<T>() << ", " << one<T>() << " ]]";
    return stream;
}

template class Mat3x4<float>;
template class Mat3x4<double>;

template std::ostream& operator<<(std::ostream&, const Mat3x4<float>&);
template std::ostream& operator<<(std::ostream&, const Mat3x4<double>&);
template std::ostream& operator<<(std::ostream&, const Mat3x4<int>&);
}