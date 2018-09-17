#include <glm/glm.hpp>
#include "pandora/flatbuffers/data_types_generated.h"

namespace pandora
{

serialization::Vec3 serialize(const glm::vec3& v);
serialization::Mat4 serialize(const glm::mat4& m);

glm::vec3 deserialize(const serialization::Vec3& v);
glm::mat4 deserialize(const serialization::Mat4& m);

}