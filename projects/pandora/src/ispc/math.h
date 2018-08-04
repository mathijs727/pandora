#pragma once

float copysign(float x, float y)
{
	// Slot but works (copied from https://github.com/open-watcom/open-watcom-v2/issues/8)
	return y == 0.0 ? abs(x) : abs(x) * y / (y);

	// Untested, efficient implementation
	//return ((x & 7FFFFFFF) | (y & 0x80000000));
}

int floatAsInt(float v)
{
	// Using reinterpret_cast might lead to undefined behavior because of illegal type aliasing, use memcpy instead
	// https://en.cppreference.com/w/cpp/language/reinterpret_cast#Type_aliasing
	unsigned int r;
	memcpy(&r, &v, sizeof(float));
	return r;
}

float intAsFloat(int v)
{
	// Using reinterpret_cast might lead to undefined behavior because of illegal type aliasing, use memcpy instead
	// https://en.cppreference.com/w/cpp/language/reinterpret_cast#Type_aliasing
	float r;
	memcpy(&r, &v, sizeof(float));
	return r;
}
