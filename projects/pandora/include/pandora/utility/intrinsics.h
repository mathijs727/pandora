#pragma once
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace pandora {

#ifdef _MSC_VER

inline int bitScan32(uint32_t mask)
{
	unsigned long result;
	_BitScanForward(&result, mask);
	return result;
}

inline int bitScan64(uint64_t mask)
{
	unsigned long result;
    _BitScanForward64(&result, mask);
    return result;
}

inline int bitScanReverse32(uint32_t mask)
{
	unsigned long result;
	_BitScanReverse(&result, mask);
	return result;
}

inline int bitScanReverse64(uint64_t mask)
{
	unsigned long result;
	_BitScanReverse64(&result, mask);
	return result;
}


#else

inline int bitScan64(uint64_t mask)
{
	// http://chessprogramming.wikispaces.com/BitScan
	const static int index64[64] = {
		0, 47,  1, 56, 48, 27,  2, 60,
		57, 49, 41, 37, 28, 16,  3, 61,
		54, 58, 35, 52, 50, 42, 21, 44,
		38, 32, 29, 23, 17, 11,  4, 62,
		46, 55, 26, 59, 40, 36, 15, 53,
		34, 51, 20, 43, 31, 22, 10, 45,
		25, 39, 14, 33, 19, 30,  9, 24,
		13, 18,  8, 12,  7,  6,  5, 63
	};

    const uint64_t debruijn64 = C64(0x03f79d71b4cb0a89);
    assert(bb != 0);
    bb |= bb >> 1;
    bb |= bb >> 2;
    bb |= bb >> 4;
    bb |= bb >> 8;
    bb |= bb >> 16;
    bb |= bb >> 32;
    return index64[(bb * debruijn64) >> 58];
}

inline int bitScanReverse64(uint64_t bb)
{
	// http://chessprogramming.wikispaces.com/BitScan
	const static int index64[64] = {
		0, 47,  1, 56, 48, 27,  2, 60,
		57, 49, 41, 37, 28, 16,  3, 61,
		54, 58, 35, 52, 50, 42, 21, 44,
		38, 32, 29, 23, 17, 11,  4, 62,
		46, 55, 26, 59, 40, 36, 15, 53,
		34, 51, 20, 43, 31, 22, 10, 45,
		25, 39, 14, 33, 19, 30,  9, 24,
		13, 18,  8, 12,  7,  6,  5, 63
	};

	const uint64_t debruijn64 = C64(0x03f79d71b4cb0a89);
	assert(bb != 0);
	bb |= bb >> 1;
	bb |= bb >> 2;
	bb |= bb >> 4;
	bb |= bb >> 8;
	bb |= bb >> 16;
	bb |= bb >> 32;
	return index64[(bb * debruijn64) >> 58];
}

inline int bitScan32(uint32_t mask)
{
	return bitScan64(static_cast<uint64_t>(mask));
}

inline int bitScanReverse32(uint32_t mask)
{
	return bitScanReverse64(static_cast<uint64_t>(mask));
}

#endif

}
