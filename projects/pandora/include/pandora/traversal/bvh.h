#pragma once
#include "pandora/core/pandora.h"
#include "pandora/geometry/bounds.h"
#include "pandora/utility/memory_arena_ts.h"
#include <boost/tti/has_member_function.hpp>
#include <gsl/span>

namespace pandora {
template <typename T>
struct is_bvh_Leaf_obj {
    //BOOST_TTI_HAS_MEMBER_FUNCTION(numPrimitives)
    BOOST_TTI_HAS_MEMBER_FUNCTION(getBounds)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersect)
    //static constexpr bool has_num_primitives = has_member_function_numPrimitives<T, unsigned, boost::mpl::vector<>, boost::function_types::const_qualified>::value;
    static constexpr bool has_bounds_func = has_member_function_getBounds<T, Bounds, boost::mpl::vector<>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect_ray_hit = has_member_function_intersect<T, bool, boost::mpl::vector<Ray&, RayHit&>, boost::function_types::const_qualified>::value;
    static constexpr bool value = has_intersect_ray_hit && has_bounds_func;
};

template <typename LeafObj>
class BVH {
public:
	static_assert(is_bvh_Leaf_obj<LeafObj>::value, "BVH leaf does not implement the is_bvh_Leaf_obj trait");

    virtual void build(gsl::span<LeafObj> leafs) = 0;

    virtual bool intersect(Ray& ray, RayHit& hitInfo) const = 0;
    virtual bool intersectAny(Ray& ray) const = 0;

    virtual size_t size() const = 0;
};

}
