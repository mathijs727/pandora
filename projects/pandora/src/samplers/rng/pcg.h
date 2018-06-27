#pragma once
#include <cstdint>

// PBRTv3 page 1065
// https://github.com/mmp/pbrt-v3/blob/master/src/core/rng.h
namespace pandora {
class PcgRng {
public:
    PcgRng();
    PcgRng(uint64_t sequenceIndex);

    void setSequence(uint64_t sequenceIndex);

	uint32_t uniformU32();
	uint32_t uniformU32(uint32_t b);
    float uniformFloat();
private:
	uint64_t m_state, m_inc;
};
}
