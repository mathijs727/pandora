#include "pandora/graphics_core/shape.h"

namespace pandora {

Shape::Shape(bool resident)
    : Evictable(resident)
{
}

}