#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/utility/memory_arena_ts.h"
//#include <boost/tti/has_member_function.hpp>
#include <gsl/span>
#include <optional>

namespace pandora {

using PauseableBVHInsertHandle = std::pair<uint32_t, uint64_t>;

/*template <typename T, typename UserState>
struct is_pauseable_leaf_obj {
    // Returns true if the ray exited the leaf node; false if the ray was paused
    BOOST_TTI_HAS_MEMBER_FUNCTION(getBounds)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersect)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersectAny)
    static constexpr bool has_bounds_func = has_member_function_getBounds<T, Bounds, boost::mpl::vector<>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect_rayhit = has_member_function_intersect<T, std::optional<bool>, boost::mpl::vector<Ray&, RayHit&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect_si = has_member_function_intersect<T, std::optional<bool>, boost::mpl::vector<Ray&, SurfaceInteraction&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect_any = has_member_function_intersectAny<T, std::optional<bool>, boost::mpl::vector<Ray&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool value = has_bounds_func && (has_intersect_rayhit || has_intersect_si) && has_intersect_any;
};*/

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
class PauseableBVH {
    //static_assert(is_pauseable_leaf_obj<LeafObj, UserState>::value, "PauseableBVH leaf does not implement the is_pausable_leaf_obj trait");

public:
    virtual size_t sizeBytes() const = 0;

    // Returns true if the ray exited the BVH; false if the ray was paused
    virtual std::optional<bool> intersect(Ray& ray, RayHit& hit, const HitRayState& userState) const = 0;
    virtual std::optional<bool> intersect(Ray& ray, RayHit& hit, const HitRayState& userState, PauseableBVHInsertHandle handle) const = 0;

    // Returns true if the ray exited the BVH; false if the ray was paused
    //virtual std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const HitRayState& userState) const = 0;
    //virtual std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const HitRayState& userState, PauseableBVHInsertHandle handle) const = 0;

    virtual std::optional<bool> intersectAny(Ray& ray, const AnyHitRayState& userState) const = 0;
    virtual std::optional<bool> intersectAny(Ray& ray, const AnyHitRayState& userState, PauseableBVHInsertHandle handle) const = 0;
};

}
