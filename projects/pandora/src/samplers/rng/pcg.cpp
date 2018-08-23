#include "pandora/samplers/rng/pcg.h"
#include <algorithm>
#include <limits>

// https://github.com/mmp/pbrt-v3/blob/master/src/core/rng.h
constexpr uint64_t PCG32_DEFAULT_STATE = 0x853c49e6748fea9bULL;
constexpr uint64_t PCG32_DEFAULT_STREAM = 0xda3e39cb94b95bdbULL;
constexpr uint64_t PCG32_MULT = 0x5851f42d4c957f2dULL;

namespace pandora {

PcgRng::PcgRng()
    : m_state(PCG32_DEFAULT_STATE)
    , m_inc(PCG32_DEFAULT_STREAM)
{
}

PcgRng::PcgRng(uint64_t initseq)
{
	setSequence(initseq);
}

void PcgRng::setSequence(uint64_t initseq)
{
    m_state = 0;
    m_inc = (initseq << 1u) | 1u;
    uniformU32();
    m_state += PCG32_DEFAULT_STATE;
    uniformU32();
}

uint32_t PcgRng::uniformU32()
{
    uint64_t oldstate = m_state;
    m_state = oldstate * PCG32_MULT + m_inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
}

uint32_t PcgRng::uniformU32(uint32_t b)
{
    uint32_t threshold = (~b + 1u) % b;
    while (true) {
        uint32_t r = uniformU32();
        if (r >= threshold)
            return r % b;
    }
}

float PcgRng::uniformFloat()
{
    return std::min(1.0f - std::numeric_limits<float>::epsilon(), uniformU32() * 0x1p-32f);
}
}
