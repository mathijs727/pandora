#include "pandora/flatbuffers/data_conversion.h"
#include <glm/gtc/type_ptr.hpp>

namespace pandora
{

serialization::Vec3 serialize(const glm::vec3& v)
{
    return serialization::Vec3(v.x, v.y, v.z);
}

serialization::Mat4 serialize(const glm::mat4& m)
{
    return serialization::Mat4(
        m[0][0], m[0][1], m[0][2], m[0][3],
        m[1][0], m[1][1], m[1][2], m[1][3],
        m[2][0], m[2][1], m[2][2], m[2][3],
        m[3][0], m[3][1], m[3][2], m[3][3]);
}

glm::vec3 deserialize(const serialization::Vec3& v)
{
    return glm::vec3(v.x(), v.y(), v.z());
}

glm::mat4 deserialize(const serialization::Mat4& m)
{
    glm::mat4 ret;
    ret[0][0] = m.x0();
    ret[0][1] = m.x1();
    ret[0][2] = m.x2();
    ret[0][3] = m.x3();

    ret[1][0] = m.y0();
    ret[1][1] = m.y1();
    ret[1][2] = m.y2();
    ret[1][3] = m.y3();

    ret[2][0] = m.z0();
    ret[2][1] = m.z1();
    ret[2][2] = m.z2();
    ret[2][3] = m.z3();

    ret[3][0] = m.w0();
    ret[3][1] = m.w1();
    ret[3][2] = m.w2();
    ret[3][3] = m.w3();
    return ret;
}

}