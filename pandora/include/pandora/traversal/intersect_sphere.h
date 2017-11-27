#pragma once
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/ray.h"

namespace pandora {

float intersectSphere(const Ray& ray, const Sphere& sphere);
}