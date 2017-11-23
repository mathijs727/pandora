#pragma once
#include "pandora/math/mat3x4.h"
#include "pandora/math/quaternion.h"
#include "pandora/math/vec3.h"

namespace pandora {

template <typename T>
class Transform {
public:
    Transform();

    void rotateX(T angle);
    void rotateY(T angle);
    void rotateZ(T angle);
    void rotate(Vec3<T> axis, T angle);

    void translate(const Vec3<T>& offset);
    void translateX(T offset);
    void translateY(T offset);
    void translateZ(T offset);

    void scale(const Vec3<T>& amount);
    void scaleX(T amount);
    void scaleY(T amount);
    void scaleZ(T amount);

    Mat3x4<T> getTransformMatrix() const;
    UnitQuaternion<T> getRotation() const;

private:
    UnitQuaternion<T> m_rotation;
    Vec3<T> m_translation;
    Vec3<T> m_scale;
};

using TransformFloat = Transform<float>;
using TransformDouble = Transform<double>;
}