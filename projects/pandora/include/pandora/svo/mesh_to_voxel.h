#pragma once
#include "pandora/core/pandora.h"

namespace pandora
{

void meshToVoxelGrid(VoxelGrid& voxelGrid, const Bounds& gridBounds, const TriangleMesh& mesh);

}