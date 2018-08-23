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
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersect)
    BOOST_TTI_HAS_MEMBER_FUNCTION(intersectAny)
    static constexpr bool hasIntersect = has_member_function_intersect<T, std::optional<bool>, boost::mpl::vector<Ray&, RayHit&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool hasIntersectAny = has_member_function_intersectAny<T, std::optional<bool>, boost::mpl::vector<Ray&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
    static constexpr bool value = hasIntersect && hasIntersectAny;
};

template <typename LeafObj, typename UserState>
class PauseableBVH {
    static_assert(is_pauseable_leaf_obj<LeafObj, UserState>::value, "PauseableBVH leaf does not implement the is_pausable_leaf_obj trait");

public:
    // Returns true if the ray exited the BVH; false if the ray was paused
    virtual std::optional<bool> intersect(Ray& ray, RayHit& hit, const UserState& userState) const = 0;
    virtual std::optional<bool> intersect(Ray& ray, RayHit& hit, const UserState& userState, PauseableBVHInsertHandle handle) const = 0;

    virtual std::optional<bool> intersectAny(Ray& ray, const UserState& userState) const = 0;
    virtual std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle handle) const = 0;
};

}
