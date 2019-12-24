#pragma once
#include "pandora/graphics_core/texture.h"
#include <memory>
#include <filesystem>

namespace pandora {

template <typename T>
class ImageTexture : public Texture<T> {
public:
    ImageTexture(std::filesystem::path filePath);
    ~ImageTexture() = default;

    T evaluate(const glm::vec2& point) const;
    T evaluate(const SurfaceInteraction& intersection) const final;

private:
    glm::ivec2 m_resolution;
    glm::vec2 m_resolutionF;
    int m_channels;

    std::unique_ptr<float[]> m_pixels;
};

}
