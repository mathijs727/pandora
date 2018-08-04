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
	int r;
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

uint8_t popcount8(uint8_t b)
{
	// https://stackoverflow.com/questions/30688465/how-to-check-the-number-of-set-bits-in-an-8-bit-unsigned-char
	b = b - ((b >> 1) & 0x55);
	b = (b & 0x33) + ((b >> 2) & 0x33);
	return (((b + (b >> 4)) & 0x0F) * 0x01);
}

int32_t popcount32(int32_t i)
{
	// https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

uint32_t popcount32(uint32_t i)
{
	// https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
