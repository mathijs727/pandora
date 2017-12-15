#pragma once
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/ray.h"

namespace pandora {

bool intersectSphere(const Sphere& sphere, Ray& ray, ShadeData& shadeData);
}