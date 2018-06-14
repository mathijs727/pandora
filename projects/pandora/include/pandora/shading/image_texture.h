#pragma once
#include "pandora/shading/texture.h"
#include <memory>

namespace pandora {

class ImageTexture : public Texture {
public:
    ImageTexture(std::string_view filename);
    ~ImageTexture() = default;

    glm::vec3 evaluate(const IntersectionData& intersection) final;

private:
    glm::ivec2 m_resolution;
    glm::vec2 m_resolutionF;
    int m_channels;

    std::unique_ptr<float[]> m_pixels;
};

}