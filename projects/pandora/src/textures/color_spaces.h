#pragma once
#include "pandora/graphics_core/pandora.h"

namespace pandora {

Spectrum gammaCorrectedToLinear(const Spectrum& gammaCorrectedCol, float gamma);
Spectrum linearToGammaCorrected(const Spectrum& linearCol, float gamma);

Spectrum approximateSRgbToLinear(const Spectrum& sRGBCol);
Spectrum approximateLinearToSRGB(const Spectrum& linearCol);

Spectrum accurateSRGBToLinear(const Spectrum& sRGBCol);
Spectrum accurateLinearToSRGB(const Spectrum& linearCol);
}
