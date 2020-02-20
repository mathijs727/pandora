#include "pandora/graphics_core/output.h"
#include <atomic>
#include <cnl/fixed_point.h>
#include <glm/common.hpp>
#include <glm/vector_relational.hpp>
#include <mutex>
#include <numeric>
// OpenImageIO should be included after cnl to prevent errors.
#include <OpenImageIO/imageio.h>

namespace pandora::detail {

using FixedPoint = cnl::fixed_point<uint64_t, -24>;

template <>
struct Pixel<float> {
    Pixel& operator=(const Pixel&);

    void add(float v);
    void min(float v);
    void max(float v);

    operator float() const;

private:
    std::mutex mutex;
    FixedPoint value { 0.0f };
    float filterWeightSum { 0.0f };
};
template <>
struct Pixel<uint64_t> {
    Pixel& operator=(const Pixel&);

    void add(uint64_t v);
    void min(uint64_t v);
    void max(uint64_t v);

    operator uint64_t() const;

private:
    std::atomic_uint64_t value { 0 };
};

}

namespace pandora {

template <typename T, AOVOperator Op>
ArbitraryOutputVariable<T, Op>::ArbitraryOutputVariable(const glm::uvec2& resolution)
    : m_resolution(resolution)
    , m_pixels(static_cast<size_t>(resolution.x) * resolution.y)
{
    clear();
}

// Implemented in CPP so we don't have to define Pixel in header (which uses cnl, which messes with the fmt library).
template <typename T, AOVOperator Op>
ArbitraryOutputVariable<T, Op>::ArbitraryOutputVariable(const ArbitraryOutputVariable& other)
    : m_resolution(other.m_resolution)
    , m_pixels(other.m_pixels.size())
{
}

template <typename T, AOVOperator Op>
ArbitraryOutputVariable<T, Op>::~ArbitraryOutputVariable() {};

template <typename T, AOVOperator Op>
void ArbitraryOutputVariable<T, Op>::addSplat(const glm::vec2& pFilm, T value)
{
    if constexpr (Op == AOVOperator::Add) {
        assert(!m_pFilter); // Filter not supported with min/max
        getPixel(glm::ivec2(pFilm)).add(value);
    } else if constexpr (Op == AOVOperator::Min) {
        assert(!m_pFilter); // Filter not supported with min/max
        getPixel(glm::ivec2(pFilm)).min(value);
    } else {
        assert(!m_pFilter); // Filter not supported with min/max
        getPixel(glm::ivec2(pFilm)).max(value);
    }
}

template <typename T, AOVOperator Op>
void ArbitraryOutputVariable<T, Op>::writeImage(std::filesystem::path file) const
{
    std::unique_ptr<OIIO::ImageOutput> pOut = OIIO::ImageOutput::create(file.string());
    if (!pOut) {
        // TODO: show error message. Currently not possible because spdlog uses fmt and a different
        //  version of fmt is already included by OpenImageIO.
        std::cerr << "Could not create output file " << file << std::endl;
        return;
    }

    std::vector<T> pixels;
    pixels.resize(static_cast<size_t>(m_resolution.x) * static_cast<size_t>(m_resolution.y));
    std::transform(std::begin(m_pixels), std::end(m_pixels), std::begin(pixels),
        [](const detail::Pixel<T>& pixel) {
            return static_cast<T>(pixel);
        });
    if constexpr (std::is_same_v<T, uint64_t>) {
        T sum = std::accumulate(std::begin(pixels), std::end(pixels), T(0));

        // EXR format only supports uint32 values.
        std::vector<uint32_t> u32Pixels;
        u32Pixels.resize(pixels.size());
        std::transform(std::begin(pixels), std::end(pixels), std::begin(u32Pixels),
            [](uint64_t p) { return static_cast<uint32_t>(p); });

        OIIO::ImageSpec spec { static_cast<int>(m_resolution.x), static_cast<int>(m_resolution.y), 1, OIIO::TypeDesc::UINT32 };
        spec.attribute("oiio:ColorSpace", "linear");
        pOut->open(file.string(), spec);
        pOut->write_image(OIIO::TypeDesc::UINT32, u32Pixels.data());
        pOut->close();
    } else if constexpr (std::is_same_v<T, float>) {
        OIIO::ImageSpec spec { static_cast<int>(m_resolution.x), static_cast<int>(m_resolution.y), 1, OIIO::TypeDesc::FLOAT };
        spec.attribute("oiio:ColorSpace", "linear");
        pOut->open(file.string(), spec);
        pOut->write_image(OIIO::TypeDesc::FLOAT, pixels.data());
        pOut->close();
    } else {
        static_assert(false);
    }
}

template <typename T, AOVOperator Op>
void ArbitraryOutputVariable<T, Op>::clear()
{
    std::fill(
        std::begin(m_pixels),
        std::end(m_pixels),
        detail::Pixel<T> {});
}

template <typename T, AOVOperator Op>
detail::Pixel<T>& ArbitraryOutputVariable<T, Op>::getPixel(const glm::ivec2& p)
{
    const int offset = p.y * m_resolution.x + p.x;
    return m_pixels[offset];
}
}

namespace pandora::detail {

Pixel<float>& Pixel<float>::operator=(const Pixel<float>& other)
{
    value = other.value;
    filterWeightSum = other.filterWeightSum;
    return *this;
}

void Pixel<float>::add(float v)
{
    std::scoped_lock l { mutex };
    value = value + FixedPoint(v);
    filterWeightSum += 1.0f;
}

void Pixel<float>::min(float v)
{
    std::scoped_lock l { mutex };
    value = std::min(static_cast<float>(value), v);
    filterWeightSum = 1.0f;
}

void Pixel<float>::max(float v)
{
    std::scoped_lock l { mutex };
    value = std::max(static_cast<float>(value), v);
    filterWeightSum = 1.0f;
}

Pixel<float>::operator float() const
{
    if (filterWeightSum > 0.0f)
        return static_cast<float>(value) / filterWeightSum;
    else
        return 0.0f;
}

Pixel<uint64_t>& Pixel<uint64_t>::operator=(const Pixel<uint64_t>& other)
{
    value.store(other.value.load());
    return *this;
}

void Pixel<uint64_t>::add(uint64_t v)
{
    value.fetch_add(v, std::memory_order_relaxed);
}

void Pixel<uint64_t>::min(uint64_t v)
{
    uint64_t oldValue, newValue;
    do {
        oldValue = value.load();
        newValue = std::min(oldValue, v);
        if (newValue == oldValue)
            break;
    } while (value.compare_exchange_weak(oldValue, newValue, std::memory_order_relaxed));
}

void Pixel<uint64_t>::max(uint64_t v)
{
    uint64_t oldValue, newValue;
    do {
        oldValue = value.load();
        newValue = std::max(oldValue, v);
        if (newValue == oldValue)
            break;
    } while (value.compare_exchange_weak(oldValue, newValue, std::memory_order_relaxed));
}

Pixel<uint64_t>::operator uint64_t() const
{
    return value.load(std::memory_order_relaxed);
}

}

namespace pandora {

template class ArbitraryOutputVariable<float, AOVOperator::Add>;

template class ArbitraryOutputVariable<uint64_t, AOVOperator::Add>;
template class ArbitraryOutputVariable<uint64_t, AOVOperator::Min>;
template class ArbitraryOutputVariable<uint64_t, AOVOperator::Max>;

}
