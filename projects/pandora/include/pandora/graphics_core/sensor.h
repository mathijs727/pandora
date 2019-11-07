#pragma once
#include <atomic>
#include <cnl/fixed_point.h>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <memory>
#include <vector>

namespace pandora {

class Sensor {
public:
    Sensor(glm::ivec2 resolution);

    void clear(glm::vec3 color);
    void addPixelContribution(glm::ivec2 pixel, glm::vec3 value);

    glm::vec3 getPixelValue(glm::ivec2 pixel) const;

    glm::ivec2 getResolution() const;
    const std::vector<glm::vec3> copyFrameBufferVec3() const;

private:
    int getIndex(int x, int y) const;

private:
    glm::ivec2 m_resolution;

	// Store pixels in fixed point format such that accumulation is deterministic independent of the order in which
	// addPixelContribution is called.
    struct Pixel {
        // 40 integer and 24 fraction bits (s40:24)
        cnl::fixed_point<uint64_t, -24> red { 0.0f };
        cnl::fixed_point<uint64_t, -24> green { 0.0f };
        cnl::fixed_point<uint64_t, -24> blue { 0.0f };

		Pixel() = default;
        Pixel(const glm::vec3& color);

        operator glm::vec3() const;
        Pixel operator+(const Pixel& other) const;
    };
    std::vector<std::atomic<Pixel>> m_frameBuffer;
};
}
