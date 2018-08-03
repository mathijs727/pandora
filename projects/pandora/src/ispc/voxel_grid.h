#pragma once

inline int voxelGrid_bitPosition(const vec3i v, int resolution)
{
	assert(v.x >= 0 && v.x < resolution);
	assert(v.y >= 0 && v.y < resolution);
	assert(v.z >= 0 && v.z < resolution);
	return v.z * resolution * resolution + v.y * resolution + v.x;
}

inline void voxelGrid_setTrue(uniform unsigned int* uniform voxelGrid, uniform int bitPosition)
{
	uniform int block = bitPosition >> 5;
	uniform int bitInBlock = bitPosition & ((1 << 5) - 1);
	voxelGrid[block] |= (1 << bitInBlock);
}