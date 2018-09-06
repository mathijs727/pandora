#pragma once
#include <glm/glm.hpp>

namespace pandora
{

class Transform
{
public:
    Transform(const glm::mat4& matrix);

    glm::mat4 getMatrix() const;
    glm::mat4 getInverseMatrix() const;
private:
    glm::mat4 m_matrix;
    glm::mat4 m_inverseMatrix;
};

}