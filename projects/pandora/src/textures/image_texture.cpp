#include "pandora/textures/image_texture.h"
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/utility/error_handling.h"
#include "textures/color_spaces.h"
#include <OpenImageIO/imageio.h>
#include <string>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

using namespace std::string_literals;

namespace pandora {

template <class T>
ImageTexture<T>::ImageTexture(std::filesystem::path filePath)
{
    ALWAYS_ASSERT(std::filesystem::exists(filePath));
    auto in = OIIO::ImageInput::open(filePath.string());
    if (!in) {
        THROW_ERROR("Could not open texture file "s + filePath.string());
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

    // Convert input to linear color space
    if constexpr (std::is_same_v<T, Spectrum>) {
        if (auto paramValue = spec.find_attribute("oiio:ColorSpace"); paramValue) {
            auto colorSpace = paramValue->get_string();
			LOG_INFO("Color space: "s + colorSpace);

            bool doConversion = false;
            std::function<Spectrum(const Spectrum&)> conversionFunction;
            if (colorSpace == "GammaCorrected") {
                float gamma = spec.get_float_attribute("oiio:Gamma");
                conversionFunction = [gamma](const Spectrum& p) {
                    return gammaCorrectedToLinear(p, gamma);
                };
                doConversion = true;
            } else if (colorSpace == "GammaCorrected2.2") {
                conversionFunction = [](const Spectrum& p) {
                    return gammaCorrectedToLinear(p, 2.2f);
                };
                doConversion = true;
            } else if (colorSpace == "sRGB") {
                conversionFunction = accurateSRGBToLinear;
                doConversion = true;
            } else if (colorSpace == "linear" || colorSpace == "Linear") {
                doConversion = false;
            } else {
                LOG_WARNING("Unsupported color space \""s + colorSpace + "\"! Reinterpreting as linear."s);
                doConversion = false;
            }

            if (doConversion) {
                int numPixels = m_resolution.x * m_resolution.y;
                tbb::parallel_for(tbb::blocked_range<int>(0, numPixels), [&](tbb::blocked_range<int> localRange) {
                    for (int i = localRange.begin(); i < localRange.end(); i++) {
                        Spectrum& s = reinterpret_cast<Spectrum&>(m_pixels[i * m_channels]);
                        s = conversionFunction(s);
                    }
                });
            }

        } else {
            LOG_WARNING("Unknown color space! Assuming linear.");
        }
    }

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
