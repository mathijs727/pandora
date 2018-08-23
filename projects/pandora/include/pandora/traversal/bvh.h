#pragma once
#include "pandora/core/pandora.h"
#include "pandora/geometry/bounds.h"
#include "pandora/utility/memory_arena_ts.h"
#include <boost/tti/has_member_function.hpp>
#include <gsl/span>

namespace pandora {
template <typename T>
struct is_bvh_Leaf_obj {
    BOOST_TTI_HAS_MEMBER_FUNCTION(numPrimitives)
    BOOST_TTI_HAS_MEMBER_FUNCTION(getPrimitiveBounds)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersectPrimitive)
    static constexpr bool has_num_primitives = has_member_function_numPrimitives<T, unsigned, boost::mpl::vector<>, boost::function_types::const_qualified>::value;
    static constexpr bool has_primitive_bounds = has_member_function_getPrimitiveBounds<T, Bounds, boost::mpl::vector<unsigned>, boost::function_types::const_qualified>::value;
    //static constexpr bool has_intersect_si = has_member_function_intersectPrimitive<T, bool, boost::mpl::vector<Ray&, SurfaceInteraction&, unsigned>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect_ray_hit = has_member_function_intersectPrimitive<T, bool, boost::mpl::vector<Ray&, RayHit&, unsigned>, boost::function_types::const_qualified>::value;
    static constexpr bool value = has_num_primitives && has_primitive_bounds && has_intersect_ray_hit;
};

template <typename LeafObj>
class BVH {
public:
	static_assert(is_bvh_Leaf_obj<LeafObj>::value, "BVH leaf does not implement the is_bvh_Leaf_obj trait");

    virtual void build(gsl::span<const LeafObj*> objects) = 0;

    virtual bool intersect(Ray& ray, RayHit& hitInfo) const = 0;
    virtual bool intersectAny(Ray& ray) const = 0;
};

}
