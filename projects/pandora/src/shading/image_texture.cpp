#include "pandora/shading/image_texture.h"
#include "pandora/traversal/ray.h"
#include <OpenImageIO/imageio.h>
#include <string>

using namespace std::string_literals;

namespace pandora {

ImageTexture::ImageTexture(std::string_view filename)
{
    auto in = OIIO::ImageInput::open(std::string(filename));
    if (!in)
        throw std::runtime_error("Could not open texture file "s + std::string(filename));

    const auto& spec = in->spec();
    m_resolution = glm::ivec2(spec.width, spec.height);
    m_resolutionF = m_resolution;
    m_channels = spec.nchannels;
    assert(m_channels >= 3);

    m_pixels = std::make_unique<float[]>(m_resolution.x * m_resolution.y * m_channels);
    in->read_image(OIIO::TypeDesc::FLOAT, m_pixels.get());// Converts input to float
    in->close();
}

glm::vec3 ImageTexture::evaluate(const IntersectionData& intersection)
{
    glm::vec2 textureCoord = intersection.uv;
    int x = std::max(0, std::min(static_cast<int>(textureCoord.x * m_resolutionF.x + 0.5f), m_resolution.x));
    int y = std::max(0, std::min(static_cast<int>(textureCoord.x * m_resolutionF.x + 0.5f), m_resolution.y));

    int pixelSize = m_channels * sizeof(float);
    const float* data = &m_pixels[(y * m_resolution.x + x) * pixelSize];
    return glm::vec3(data[0], data[1], data[2]);
}

}
