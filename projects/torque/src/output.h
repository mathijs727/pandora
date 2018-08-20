#pragma once
#include "pandora/core/pandora.h"
#include <string_view>

namespace torque
{

void writeOutputToFile(const pandora::Sensor& sensor, std::string_view fileName);

}