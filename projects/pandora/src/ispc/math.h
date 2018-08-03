#pragma once

typedef float<2> vec2;
typedef float<3> vec3;
typedef int<3> vec3i;

struct CPPVec3
{
	float x, y, z;
};

struct CPPVec3i
{
	int x, y, z;
};

vec3 make_vec3(float x, float y, float z);
vec3i make_vec3i(int x, int y, int z);

inline vec3 make_vec3(CPPVec3 v)
{
	return make_vec3(v.x, v.y, v.z);
}

inline vec3i make_vec3i(CPPVec3i v)
{
	return make_vec3i(v.x, v.y, v.z);
}

inline vec2 make_vec2(float x, float y)
{
	vec2 result = { x, y };
	return result;
}

inline vec3 make_vec3(float x, float y, float z)
{
	vec3 result = { x, y, z };
	return result;
}

inline vec3i make_vec3i(int x, int y, int z)
{
	vec3i result = { x, y, z };
	return result;
}

inline uniform vec3i make_vec3i(uniform int x, uniform int y, uniform int z)
{
	uniform vec3i result = { x, y, z };
	return result;
}

inline uniform vec3i make_vec3i(uniform int v)
{
	uniform vec3i result = { v, v, v };
	return result;
}

inline float dot(vec2 a, vec2 b)
{
	return a.x * b.x + a.y * b.y;
}

inline float dot(vec3 a, vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline vec3 min(const vec3 a, const vec3 b)
{
	float r0 = min(a.x, b.x);
	float r1 = min(a.y, b.y);
	float r2 = min(a.z, b.z);
	vec3 result = { r0, r1, r2 };
	return result;
}

inline vec3 max(const vec3 a, const vec3 b)
{
	float r0 = max(a.x, b.x);
	float r1 = max(a.y, b.y);
	float r2 = max(a.z, b.z);
	vec3 result = { r0, r1, r2 };
	return result;
}

inline vec3 cross(vec3 u, vec3 v)
{
	// https://en.wikipedia.org/wiki/Cross_product
	float r0 = u[1] * v[2] - u[2] * v[1];
	float r1 = u[2] * v[0] - u[0] * v[2];
	float r2 = u[0] * v[1] - u[1] * v[0];

	vec3 result = { r0, r1, r2 };
	return result;
}

inline uniform float maxComponent(const uniform vec3 v)
{
	return max(v.x, max(v.y, v.z));
}
