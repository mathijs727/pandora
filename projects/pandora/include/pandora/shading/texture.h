#pragma once
#include "glm/glm.hpp"
#include <string_view>

namespace pandora {

struct SurfaceInteraction;

class Texture {
public:
    virtual glm::vec3 evaluate(const SurfaceInteraction& intersection) = 0;
};

}
