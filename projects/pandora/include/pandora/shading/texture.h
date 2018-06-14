#pragma once
#include "glm/glm.hpp"
#include <string_view>

namespace pandora {

struct IntersectionData;

class Texture {
public:
    virtual glm::vec3 evaluate(const IntersectionData& intersection) = 0;
};

}
