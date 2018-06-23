#include "pandora/textures/image_texture.h"
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include <OpenImageIO/imageio.h>
#include <string>

using namespace std::string_literals;

namespace pandora {

template <class T>
ImageTexture<T>::ImageTexture(std::string_view filename)
{
    auto in = OIIO::ImageInput::open(std::string(filename));
	if (!in)
	{
		std::cerr << "Could not open texture file " << filename << std::endl;
        throw std::runtime_error("Could not open texture file "s + std::string(filename));
	}

    const auto& spec = in->spec();
    m_resolution = glm::ivec2(spec.width, spec.height);
    m_resolutionF = m_resolution;
    m_channels = spec.nchannels;
    if constexpr (std::is_same_v<T, float>)
        assert(m_channels >= 1);
    if constexpr (std::is_same_v<T, glm::vec3>)
        assert(m_channels >= 3);

    m_pixels = std::make_unique<float[]>(m_resolution.x * m_resolution.y * m_channels);
    in->read_image(OIIO::TypeDesc::FLOAT, m_pixels.get()); // Converts input to float
    in->close();
}

template <class T>
T ImageTexture<T>::evaluate(const glm::vec2& point) const
{
    glm::vec2 textureCoord = point;
    int x = std::max(0, std::min(static_cast<int>(textureCoord.x * m_resolutionF.x + 0.5f), m_resolution.x - 1));
    int y = std::max(0, std::min(static_cast<int>(textureCoord.y * m_resolutionF.y + 0.5f), m_resolution.y - 1));

    int pixelSize = m_channels;
    const float* data = &m_pixels[(y * m_resolution.x + x) * pixelSize];
    if constexpr (std::is_same_v<T, float>)
        return data[0];
    else if constexpr (std::is_same_v<T, glm::vec3>)
        return glm::vec3(data[0], data[1], data[2]);
    else
        static_assert("Unknown template type in ImageTexture");
}

template <class T>
T ImageTexture<T>::evaluate(const SurfaceInteraction& surfaceInteraction) const
{
    // TODO: mip mapping and ray differentials
    return evaluate(surfaceInteraction.uv);
}

// Explicit instantiation
template class ImageTexture<float>;
template class ImageTexture<glm::vec3>;

}
