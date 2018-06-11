#pragma once
#include "glm/glm.hpp"
#include "pandora/geometry/bounds.h"
#include "pandora/traversal/ray.h"

#include <gsl/gsl>

namespace pandora {
class Shape {
public:
    virtual unsigned numPrimitives() const = 0;
};
}
