#pragma once
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

namespace pandora {

enum class AOVOperator {
    Add,
    Min,
    Max
};

namespace detail {
    template <typename T>
    struct Pixel;
}

template <typename T, AOVOperator Op>
class ArbitraryOutputVariable {
public:
    ArbitraryOutputVariable(const glm::uvec2& resolution);
    ArbitraryOutputVariable(const ArbitraryOutputVariable&);
    ~ArbitraryOutputVariable();

    // Pixel on a scale of 0 to 1
    void addSplat(const glm::vec2& pixel, T value);
    void writeImage(std::filesystem::path file) const;
    void clear();

private:
    detail::Pixel<T>& getPixel(const glm::ivec2& p);

private:
    // NOTE: integer instead of unsigned to prevent underflow issues with pixels located "below" the origin.
    const glm::ivec2 m_resolution;

    std::vector<detail::Pixel<T>> m_pixels;
};

}