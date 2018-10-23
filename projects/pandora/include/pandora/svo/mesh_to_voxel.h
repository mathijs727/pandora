#pragma once
#include "pandora/core/pandora.h"

namespace pandora
{

void sceneObjectToVoxelGrid(VoxelGrid& voxelGrid, const Bounds& gridBounds, const InstancedSceneObjectGeometry& sceneObjectGeometry);

}