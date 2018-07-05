#pragma once
#include "pandora/core/pandora.h"
#include "pandora/utility/memory_arena_ts.h"
#include <gsl/span>

namespace pandora
{

template <typename LeafObj>
class BVH
{
public:
	virtual void build(gsl::span<const LeafObj*> objects) = 0;

	virtual bool intersect(Ray& ray, SurfaceInteraction& si) const = 0;
};

}
