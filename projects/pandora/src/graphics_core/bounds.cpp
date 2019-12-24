#include "pandora/graphics_core/bounds.h"
#include "pandora/flatbuffers/data_conversion.h"
#include <algorithm>
#include <embree3/rtcore.h>

namespace pandora {

Bounds::Bounds()
    : min(std::numeric_limits<float>::max())
    , max(std::numeric_limits<float>::lowest())
{
}

Bounds::Bounds(glm::vec3 lower, glm::vec3 upper)
    : min(lower)
    , max(upper)
{
}

Bounds::Bounds(const RTCBounds& bounds)
    : min(bounds.lower_x, bounds.lower_y, bounds.lower_z)
    , max(bounds.upper_x, bounds.upper_y, bounds.upper_z)
{
}

Bounds::Bounds(const serialization::Bounds& serialized)
    : min(deserialize(serialized.min()))
    , max(deserialize(serialized.max()))
{
}

Bounds& Bounds::operator*=(const glm::mat4& matrix)
{
    const glm::vec3 v0 = matrix * glm::vec4(min.x, min.y, min.z, 1.0f);
    const glm::vec3 v1 = matrix * glm::vec4(min.x, min.y, max.z, 1.0f);
    const glm::vec3 v2 = matrix * glm::vec4(min.x, max.y, min.z, 1.0f);
    const glm::vec3 v3 = matrix * glm::vec4(min.x, max.y, max.z, 1.0f);
    const glm::vec3 v4 = matrix * glm::vec4(max.x, min.y, min.z, 1.0f);
    const glm::vec3 v5 = matrix * glm::vec4(max.x, min.y, max.z, 1.0f);
    const glm::vec3 v6 = matrix * glm::vec4(max.x, max.y, min.z, 1.0f);
    const glm::vec3 v7 = matrix * glm::vec4(max.x, max.y, max.z, 1.0f);
    this->min = glm::min(v0, glm::min(v1, glm::min(v2, glm::min(v3, glm::min(v4, glm::min(v5, glm::min(v6, v7)))))));
    this->max = glm::max(v0, glm::max(v1, glm::max(v2, glm::max(v3, glm::max(v4, glm::max(v5, glm::max(v6, v7)))))));
    return *this;
}

bool Bounds::operator==(const Bounds& other) const
{
    return min == other.min && max == other.max;
}

void Bounds::reset()
{
    min = glm::vec3(std::numeric_limits<float>::max());
    max = glm::vec3(std::numeric_limits<float>::lowest());
}

void Bounds::grow(glm::vec3 vec)
{
    min = glm::min(min, vec);
    max = glm::max(max, vec);
}

void Bounds::extend(const Bounds& other)
{
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

Bounds Bounds::extended(const Bounds& other) const
{
    auto minBounds = glm::min(min, other.min);
    auto maxBounds = glm::max(max, other.max);
    return { minBounds, maxBounds };
}

glm::vec3 Bounds::center() const
{
    return (min + max) / 2.0f;
}

glm::vec3 Bounds::extent() const
{
    return max - min;
}

float Bounds::surfaceArea() const
{
    glm::vec3 extent = max - min;
    return 2.0f * (extent.x * extent.y + extent.y * extent.z + extent.z * extent.x);
}

Bounds Bounds::intersection(const Bounds& other) const
{
    return Bounds(glm::max(min, other.min), glm::min(max, other.max));
}

bool Bounds::overlaps(const Bounds& other) const
{
    glm::vec3 extent = intersection(other).extent();
    return (extent.x >= 0.0f && extent.y >= 0.0f && extent.z >= 0.0f);
}

bool Bounds::intersect(const Ray& ray, float& tmin, float& tmax) const
{
    tmin = 0.0f;
    tmax = std::numeric_limits<float>::max();

    if (ray.direction.x != 0.0f) {
        float tx1 = (min.x - ray.origin.x) / ray.direction.x;
        float tx2 = (max.x - ray.origin.x) / ray.direction.x;

        tmin = std::max(tmin, std::min(tx1, tx2));
        tmax = std::min(tmax, std::max(tx1, tx2));
    }

    if (ray.direction.y != 0.0f) {
        float ty1 = (min.y - ray.origin.y) / ray.direction.y;
        float ty2 = (max.y - ray.origin.y) / ray.direction.y;

        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));
    }

    if (ray.direction.z != 0.0f) {
        float tz1 = (min.z - ray.origin.z) / ray.direction.z;
        float tz2 = (max.z - ray.origin.z) / ray.direction.z;

        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));
    }

    // tmax >= 0: prevent boxes before the starting position from being hit
    // See the comment section at:
    //  https://tavianator.com/fast-branchless-raybounding-box-intersections/
    return tmax >= tmin && tmax >= 0.0f;
}

bool Bounds::contains(const glm::vec3& point) const
{
    return glm::all(glm::greaterThanEqual(point, min)) && glm::all(glm::lessThanEqual(point, max));
}

bool Bounds::contains(const Bounds& other) const
{
    return contains(other.min) && contains(other.max);
}

serialization::Bounds Bounds::serialize() const
{
    return serialization::Bounds(pandora::serialize(min), pandora::serialize(max));
}

}
