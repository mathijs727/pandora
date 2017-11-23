#include "pandora/math/transform.h"
#include "pandora/math/constants.h"
#include <type_traits>

namespace pandora {
//void rotate(Quaternion<T> q);
template <typename T>
Transform<T>::Transform()
    : m_rotation(UnitQuaternion<T>::rotation(Vec3<T>(one<T>()), zero<T>()))
    , m_translation(Vec3<T>(zero<T>()))
    , m_scale(Vec3<T>(one<T>()))
{
}

template <typename T>
void Transform<T>::rotateX(T angle)
{
    auto axis = Vec3<T>(one<T>(), zero<T>(), zero<T>());
    auto rotation = UnitQuaternion<T>::rotation(axis, angle);
    m_rotation = rotation * m_rotation;
}

template <typename T>
void Transform<T>::rotateY(T angle)
{
    auto axis = Vec3<T>(zero<T>(), one<T>(), zero<T>());
    auto rotation = UnitQuaternion<T>::rotation(axis, angle);
    m_rotation = rotation * m_rotation;
}

template <typename T>
void Transform<T>::rotateZ(T angle)
{
    auto axis = Vec3<T>(zero<T>(), zero<T>(), one<T>());
    auto rotation = UnitQuaternion<T>::rotation(axis, angle);
    m_rotation = rotation * m_rotation;
}

template <typename T>
void Transform<T>::rotate(Vec3<T> axis, T angle)
{
    auto rotation = UnitQuaternion<T>::rotation(axis, angle);
    m_rotation = rotation * m_rotation;
}

template <typename T>
void Transform<T>::translate(const Vec3<T>& offset)
{
    m_translation += offset;
}

template <typename T>
void Transform<T>::translateX(T offset)
{
    m_translation.x += offset;
}

template <typename T>
void Transform<T>::translateY(T offset)
{
    m_translation.y += offset;
}

template <typename T>
void Transform<T>::translateZ(T offset)
{
    m_translation.z += offset;
}

template <typename T>
void Transform<T>::scale(const Vec3<T>& amount)
{
    m_scale *= amount;
}

template <typename T>
void Transform<T>::scaleX(T amount)
{
    m_scale.x *= amount;
}

template <typename T>
void Transform<T>::scaleY(T amount)
{
    m_scale.y *= amount;
}

template <typename T>
void Transform<T>::scaleZ(T amount)
{
    m_scale.z *= amount;
}

template <typename T>
Mat3x4<T> Transform<T>::getTransformMatrix() const
{
    auto scaleMatrix = Mat3x4<T>::scale(m_scale);
    auto rotationMatrix = m_rotation.rotationMatrix();
    auto translationMatrix = Mat3x4<T>::translation(m_translation);
    return translationMatrix * rotationMatrix * scaleMatrix;
}

template <typename T>
UnitQuaternion<T> Transform<T>::getRotation() const
{
    return m_rotation;
}

template class Transform<float>;
template class Transform<double>;
}
