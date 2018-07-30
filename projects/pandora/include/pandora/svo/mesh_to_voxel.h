#pragma once
#include "pandora/svo/voxel_grid.h"
#include "pandora/core/pandora.h"

namespace pandora
{

void meshToVoxelGridNaive(VoxelGrid& voxelGrid, const TriangleMesh& mesh, int resolution);

}