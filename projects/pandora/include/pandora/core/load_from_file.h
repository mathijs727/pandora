#pragma once
#include <string_view>
#include "pandora/core/scene.h"

namespace pandora
{

struct RenderConfig
{
    Scene scene;
};

RenderConfig loadFromFile(std::string_view filename);

}