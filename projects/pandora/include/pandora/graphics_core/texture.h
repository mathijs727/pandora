#pragma once
#include "glm/glm.hpp"
#include <string_view>

namespace pandora {

struct SurfaceInteraction;

template <typename T>
class Texture {
public:
    virtual ~Texture() = default;
    virtual T evaluate(const glm::vec2& point) const = 0;
    virtual T evaluate(const SurfaceInteraction& intersection) const = 0;
};

}
