#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"

namespace pandora {

struct SurfaceInteraction {
public:
    SurfaceInteraction(
        const Shape* shape_,
        unsigned primitiveIndex_,
        const Point3f& p_,
        const Vec3f& wo_,
        const Point2f& uv_,
        const Vec3f& dpdu_, const Vec3f& dpdv_,
        const Normal3f& dndu_, const Normal3f& dndv_);
public:
    const Shape* shape = nullptr;
    unsigned primitiveIndex;

    Point3f p;
    Vec3f wo;

    Normal3f n;// Geometric normal
    Point2f uv;
    Vec3f dpdu, dpdv;
    Normal3f dndu, dndv;

    struct {
        Normal3f n;// Shading normal
        Vec3f dpdu, dpdv;
        Normal3f dndu, dndv;
    } shading;
};

}
