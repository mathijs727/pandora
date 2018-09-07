#pragma once
#include "pandora/core/pandora.h"
#include "pandora/utility/memory_arena_ts.h"
#include <boost/tti/has_member_function.hpp>
#include <gsl/span>
#include <optional>

namespace pandora {

using PauseableBVHInsertHandle = std::pair<uint32_t, uint64_t>;

template <typename T, typename UserState>
struct is_pauseable_leaf_obj {
    // Returns true if the ray exited the leaf node; false if the ray was paused
    BOOST_TTI_HAS_MEMBER_FUNCTION(getBounds)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersect)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersectAny)
    static constexpr bool has_bounds_func = has_member_function_getBounds<T, Bounds, boost::mpl::vector<>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect = has_member_function_intersect<T, std::optional<bool>, boost::mpl::vector<Ray&, RayHit&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool has_intersect_any = has_member_function_intersectAny<T, std::optional<bool>, boost::mpl::vector<Ray&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool value = has_bounds_func && has_intersect && has_intersect_any;
};

template <typename LeafObj, typename UserState>
class PauseableBVH {
    static_assert(is_pauseable_leaf_obj<LeafObj, UserState>::value, "PauseableBVH leaf does not implement the is_pausable_leaf_obj trait");

public:
    virtual size_t size() const = 0;

    // Returns true if the ray exited the BVH; false if the ray was paused
    virtual std::optional<bool> intersect(Ray& ray, RayHit& hit, const UserState& userState) const = 0;
    virtual std::optional<bool> intersect(Ray& ray, RayHit& hit, const UserState& userState, PauseableBVHInsertHandle handle) const = 0;

    virtual std::optional<bool> intersectAny(Ray& ray, const UserState& userState) const = 0;
    virtual std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle handle) const = 0;
};

}
