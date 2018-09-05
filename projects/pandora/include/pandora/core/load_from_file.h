#pragma once
#include <string_view>
#include "pandora/core/scene.h"
#include "pandora/core/perspective_camera.h"
#include <glm/glm.hpp>

namespace pandora
{

struct RenderConfig
{
    std::unique_ptr<PerspectiveCamera> camera;
    glm::ivec2 resolution;
    Scene scene;
};

RenderConfig loadFromFile(std::string_view filename);

}