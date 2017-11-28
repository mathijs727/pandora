#include "pandora/traversal/intersect_sphere.h"
#include <cmath>
#include <iostream>

namespace pandora {

bool intersectSphere(const Sphere& sphere, Ray& ray)
{
    Vec3f c = sphere.center - ray.origin;
    float t = dot(c, ray.direction);
    Vec3f q = c - t * ray.direction;
    float p2 = dot(q, q);

    if (p2 > sphere.radius2)
        return false; // r2 = r * r

    t -= std::sqrt(sphere.radius2 - p2);
    if (t < ray.t && t > 0) {
        ray.t = t;
        return true;
    } else {
        return false;
    }
}
}