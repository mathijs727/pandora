#include "..\..\include\pandora\core\transform.h"
#include "pandora/core/transform.h"

namespace pandora {

Transform::Transform(const glm::mat4& matrix)
    : m_matrix(matrix)
    , m_inverseMatrix(glm::inverse(matrix))
{
}

glm::mat4 Transform::getMatrix() const
{
    return m_matrix;
}

glm::mat4 Transform::getInverseMatrix() const
{
    return m_inverseMatrix;
}

}
