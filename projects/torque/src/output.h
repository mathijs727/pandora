#pragma once
#include "pandora/graphics_core/pandora.h"
#include <string_view>

namespace torque
{

void writeOutputToFile(pandora::Sensor& sensor, int spp, std::string_view fileName, bool applyPostProcessing);

}