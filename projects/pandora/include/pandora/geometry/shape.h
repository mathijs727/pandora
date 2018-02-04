#pragma once
#include "pandora/math/bounds3.h"
#include "pandora/math/vec2.h"
#include "pandora/traversal/ray.h"

#include <gsl/gsl>

namespace pandora {
class Shape {
public:
    virtual unsigned numPrimitives() const = 0;
};
}