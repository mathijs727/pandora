#pragma once
#include "pandora/core/pandora.h"
#include "pandora/utility/memory_arena_ts.h"

namespace pandora
{

template <typename LeafNode>
class BVH
{
public:
	virtual void addObject(const LeafNode* objectPtr) = 0;
	virtual void commit() = 0;

	virtual bool intersect(Ray& ray, SurfaceInteraction& si) const = 0;
};

}
