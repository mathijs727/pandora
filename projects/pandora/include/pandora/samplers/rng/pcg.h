#pragma once
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
    uint64_t uniformU64();
    float uniformFloat();
    glm::vec2 uniformFloat2();
    glm::vec3 uniformFloat3();

private:
    uint64_t m_state, m_inc;
};
}
