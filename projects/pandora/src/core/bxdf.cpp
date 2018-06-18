#include "pandora/core/bxdf.h"

namespace pandora {
BxDF::BxDF(BxDFType type)
    : m_type(type)
{
}

BxDFType BxDF::getType() const
{
    return m_type;
}

bool BxDF::matchesFlags(BxDFType t) const
{
    return (m_type & t) == m_type;
}

}
