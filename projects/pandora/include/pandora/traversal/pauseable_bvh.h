#pragma once
#include "pandora/core/pandora.h"
#include "pandora/utility/memory_arena_ts.h"
#include <gsl/span>
#include <boost/tti/has_member_function.hpp>
#include <optional>

namespace pandora
{

using PauseableBVHInsertHandle = std::pair<uint32_t, uint64_t>;

template <typename T, typename UserState>
struct is_pauseable_leaf_obj {
	BOOST_TTI_HAS_MEMBER_FUNCTION(intersect)
	static constexpr bool value = has_member_function_intersect<T, std::optional<bool>, boost::mpl::vector<Ray&, SurfaceInteraction&, const UserState&, PauseableBVHInsertHandle>, boost::function_types::const_qualified>::value;
};

template <typename LeafObj, typename UserState>
class PauseableBVH
{
	static_assert(is_pauseable_leaf_obj<LeafObj, UserState>::value, "PauseableBVH leaf does not implement the is_pausable_leaf_obj trait");
public:
	virtual std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const UserState& userState) const = 0;
	virtual std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const UserState& userState, PauseableBVHInsertHandle handle) const = 0;
};

}
