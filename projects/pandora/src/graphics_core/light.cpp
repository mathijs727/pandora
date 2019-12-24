#include "pandora/graphics_core/light.h"

namespace pandora {

Light::Light(int flags)
    : m_flags(flags)
{
}

bool Light::isDeltaLight() const
{
    return m_flags & (int)LightFlags::DeltaPosition || m_flags & (int)LightFlags::DeltaDirection;
}

Spectrum Light::Le(const Ray& ray) const
{
    (void)ray;
    return Spectrum(0.0f);
}

InfiniteLight::InfiniteLight(int flags)
    : Light(flags)
{
}

}
