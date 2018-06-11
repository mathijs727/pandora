#include "pandora/shading/core/surface_interaction.h"

namespace pandora {

SurfaceInteraction::SurfaceInteraction(
    const Shape* shape_,
    unsigned primitiveIndex_,
    const Point3f& p_,
    const Vec3f& wo_,
    const Point2f& uv_,
    const Vec3f& dpdu_,
    const Vec3f& dpdv_,
    const Normal3f& dndu_,
    const Normal3f& dndv_) :
    shape(shape),
    primitiveIndex(primitiveIndex_),
    p(p_),
    wo(wo_),
    n(cross(dpdu, dpdv).normalized()),
    uv(uv_),
    dpdu(dpdu_),
    dpdv(dpdv_),
    dndu(dndu_),
    dndv(dndv_)
{
    shading.n = n;
    shading.dpdu = dpdu_;
    shading.dpdv = dpdv_;
    shading.dndu = dndu_;
    shading.dndv = dndv_;
}

}
