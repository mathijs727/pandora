#pragma once
#include "pandora/flatbuffers/data_types_generated.h"
#include <glm/glm.hpp>

namespace pandora {

serialization::Vec2 serialize(const glm::vec2& v);
serialization::Vec3 serialize(const glm::vec3& v);
serialization::Vec3i serialize(const glm::ivec3& v);
serialization::Vec3u serialize(const glm::uvec3& v);
serialization::Mat4 serialize(const glm::mat4& m);

glm::vec2 deserialize(const serialization::Vec2& v);
glm::vec3 deserialize(const serialization::Vec3& v);
glm::ivec3 deserialize(const serialization::Vec3i& v);
glm::uvec3 deserialize(const serialization::Vec3u& v);
glm::mat4 deserialize(const serialization::Mat4& m);

}
