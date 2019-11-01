#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <pandora/textures/constant_texture.h>
#include <pandora/textures/image_texture.h>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace std {

template <>
struct hash<glm::vec3> {
    size_t operator()(const glm::vec3& v) const
    {
        // https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
        std::hash<float> hasher;
        size_t seed = hasher(v.x);
        seed ^= hasher(v.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
}

class TextureCache {
public:
    template <typename T>
    std::shared_ptr<pandora::Texture<T>> getImageTexture(std::filesystem::path filePath)
    {
        if (!std::filesystem::exists(filePath)) {
            spdlog::warn("Cannot find texture \"{}\", replacing with constant value", filePath.string());
            return getConstantTexture<T>(T(0.5f));
		}

        auto& cache = std::get<ImageCache<T>>(m_imageTextures);
        if (auto iter = cache.find(filePath.string()); iter != std::end(cache)) {
            return iter->second;
        } else {
            auto pTexture = std::make_shared<pandora::ImageTexture<T>>(filePath);
            cache[filePath.string()] = pTexture;
            return pTexture;
        }
    }

    template <typename T>
    std::shared_ptr<pandora::Texture<T>> getConstantTexture(T value)
    {
        auto& cache = std::get<ConstantCache<T>>(m_constantTextures);
        if (auto iter = cache.find(value); iter != std::end(cache)) {
            return iter->second;
        } else {
            auto pTexture = std::make_shared<pandora::ConstantTexture<T>>(value);
            cache[value] = pTexture;
            return pTexture;
        }
    }

private:
    template <typename T>
    using ImageCache = std::unordered_map<std::string, std::shared_ptr<pandora::ImageTexture<T>>>;
    template <typename T>
    using ConstantCache = std::unordered_map<T, std::shared_ptr<pandora::ConstantTexture<T>>>;

    std::tuple<ImageCache<float>, ImageCache<glm::vec3>> m_imageTextures;
    std::tuple<ConstantCache<float>, ConstantCache<glm::vec3>> m_constantTextures;
};