#pragma once

struct Bounds
{
	vec3 min;
	vec3 max;
};

vec3 extent(const Bounds bounds)
{
	return bounds.max - bounds.min;
}

uniform vec3 extent(const uniform Bounds bounds)
{
	return bounds.max - bounds.min;
}