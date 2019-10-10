#pragma once
#include "pandora/graphics_core/pandora.h"
#include <filesystem>

namespace torque
{

void writeOutputToFile(pandora::Sensor& sensor, int spp, std::filesystem::path file, bool applyPostProcessing);

}