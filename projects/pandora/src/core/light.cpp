#include "pandora/core/light.h"

namespace pandora {

Light::Light(int flags, int numSamples)
    : m_flags(flags)
    , m_numSamples(std::max(1, numSamples))
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

}
