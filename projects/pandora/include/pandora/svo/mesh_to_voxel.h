#pragma once
#include "pandora/graphics_core/pandora.h"

namespace pandora
{

void sceneObjectToVoxelGrid(VoxelGrid& voxelGrid, const Bounds& gridBounds, const InstancedSceneObjectGeometry& sceneObjectGeometry);

}