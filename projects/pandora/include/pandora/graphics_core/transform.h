#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/ray.h"
#include "pandora/geometry/bounds.h"
#include "pandora/graphics_core/interaction.h"
#include "pandora/flatbuffers/data_types_generated.h"
#include <glm/glm.hpp>

namespace pandora {

class Transform {
public:
    Transform(const glm::mat4& matrix);
    Transform(const serialization::Transform* serializedTransform);

    Transform operator*(const Transform& other) const;

    serialization::Transform serialize() const;

    glm::vec3 transformPoint(const glm::vec3& p) const;
    glm::vec3 transformVector(const glm::vec3& v) const;
    glm::vec3 transformNormal(const glm::vec3& n) const;

    Ray transform(const Ray& ray) const;
    Bounds transform(const Bounds& bounds) const;
    SurfaceInteraction transform(const SurfaceInteraction& si) const;

private:
    glm::mat4 m_matrix;
    glm::mat4 m_inverseMatrix;
};

}
