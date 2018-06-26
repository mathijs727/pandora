#include "textures/color_spaces.h"

namespace pandora {

Spectrum gammaCorrectedToLinear(const Spectrum& gammaCorrectedCol, float gamma)
{
	return glm::pow(gammaCorrectedCol, Spectrum(gamma));
}

Spectrum linearToGammaCorrected(const Spectrum& linearCol, float gamma)
{
	return glm::pow(linearCol, Spectrum(1.0f / gamma));
}

// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
Spectrum approximateSRgbToLinear(const Spectrum& sRGBCol)
{
    return glm::pow(sRGBCol, Spectrum(2.2f));
}

// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
Spectrum approximateLinearToSRGB(const Spectrum& linearCol)
{
    return glm::pow(linearCol, Spectrum(1.0f / 2.2f));
}

// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
Spectrum accurateSRGBToLinear(const Spectrum& sRGBCol)
{
    Spectrum linearRGBLo = sRGBCol / 12.92f;
    Spectrum linearRGBHi = glm::pow((sRGBCol + 0.055f) / 1.055f, Spectrum(2.4f));
    Spectrum linearRGB = Spectrum(
        (sRGBCol.x <= 0.04045f) ? linearRGBLo.x : linearRGBHi.x,
        (sRGBCol.y <= 0.04045f) ? linearRGBLo.y : linearRGBHi.y,
        (sRGBCol.z <= 0.04045f) ? linearRGBLo.z : linearRGBHi.z);
    return linearRGB;
}

// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
Spectrum accurateLinearToSRGB(const Spectrum& linearCol)
{
    return Spectrum();
}

}
