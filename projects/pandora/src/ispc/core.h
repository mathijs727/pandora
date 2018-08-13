#pragma once
#include "ispc/vector_math.h"

typedef int8 int8_t;
typedef unsigned int8 uint8_t;
typedef int16 int16_t;
typedef unsigned int16 uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef int64 int64_t;
typedef unsigned int64 uint64_t;

struct RaySOA
{
	uniform float* originX;
	uniform float* originY;
	uniform float* originZ;
	uniform float* directionX;
	uniform float* directionY;
	uniform float* directionZ;
};