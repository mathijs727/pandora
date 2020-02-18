#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/ray.h"
#include <glm/gtc/matrix_transform.hpp> // glm::identity
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace pandora {

class Transform {
public:
    Transform() = default;
    Transform(const glm::mat4& localToWorld);

    Transform operator*(const Transform& other) const;

    glm::vec3 transformPointToWorld(const glm::vec3& p) const;
    glm::vec3 transformVectorToWorld(const glm::vec3& v) const;
    glm::vec3 transformNormalToWorld(const glm::vec3& n) const;

    glm::vec3 transformVectorToLocal(const glm::vec3& v) const;

    Ray transformToLocal(const Ray& ray) const;
    RayHit transformToLocal(const RayHit& hit) const;
    Bounds transformToWorld(const Bounds& bounds) const;
    Interaction transformToWorld(const Interaction& si) const;
    SurfaceInteraction transformToWorld(const SurfaceInteraction& si) const;

private:
    glm::mat4 m_matrix { glm::identity<glm::mat4>() };
    glm::mat4 m_inverseMatrix { glm::identity<glm::mat4>() };
};

}
