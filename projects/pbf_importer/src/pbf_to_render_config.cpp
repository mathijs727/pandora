#include "pbf/pbf_to_render_config.h"
#include "texture_cache.h"
#include <pandora/lights/area_light.h>
#include <pandora/lights/distant_light.h>
#include <pandora/lights/environment_light.h>
#include <pandora/materials/matte_material.h>
#include <pandora/shapes/triangle.h>
#include <spdlog/spdlog.h>

namespace pbf {

struct Converter {
    static pandora::PerspectiveCamera convertCamera(const PBFCamera* pBbfCamera, float aspectRatio);
    pandora::Scene convertWorld(const PBFObject* pWorld);

    std::shared_ptr<pandora::Shape> convertShape(const PBFShape* pShape);
    std::shared_ptr<pandora::Material> convertMaterial(const PBFMaterial* pMaterial);
    std::unique_ptr<pandora::InfiniteLight> convertInfiniteLIght(const PBFLightSource* pLight);

    template <typename T>
    std::shared_ptr<pandora::Texture<T>> getTexture(const PBFTexture* pTexture);
    template <typename T>
    std::shared_ptr<pandora::Texture<T>> getTexOptional(const PBFTexture* pTexture, T constValue);

private:
    TextureCache m_texCache;
    std::unordered_map<const PBFMaterial*, std::shared_ptr<pandora::Material>> m_materialCache;
};

pandora::RenderConfig pbfToRenderConfig(PBFScene* pScene)
{
    const glm::ivec2 resolution = pScene->pFilm->resolution;
    const float aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);

    Converter converter {};

    pandora::RenderConfig out {};
    out.camera = std::make_unique<pandora::PerspectiveCamera>(Converter::convertCamera(pScene->cameras[0], aspectRatio));
    out.resolution = resolution;
    out.pScene = std::make_unique<pandora::Scene>(converter.convertWorld(pScene->pWorld));
    return out;
}

pandora::PerspectiveCamera Converter::convertCamera(const PBFCamera* pBbfCamera, float aspectRatio)
{
    // PBRT defines field of view along shortest axis
    float fovX = pBbfCamera->fov;
    if (aspectRatio > 1.0f)
        fovX = glm::degrees(std::atan(std::tan(glm::radians(fovX / 2.0f)) * aspectRatio) * 2.0f);

    return pandora::PerspectiveCamera(aspectRatio, fovX, pBbfCamera->frame);
}

pandora::Scene Converter::convertWorld(const PBFObject* pWorld)
{
    pandora::SceneBuilder sceneBuilder;

	// TODO: instancing

    for (const auto* pPBFShape : pWorld->shapes) {
        if (!pPBFShape)
            continue;

        auto pShape = convertShape(pPBFShape);
        auto pMaterial = convertMaterial(pPBFShape->pMaterial);

        // Load area light
        if (pPBFShape->pAreaLight) {
            auto pAreaLight = std::make_unique<pandora::AreaLight>(pPBFShape->pAreaLight->L);
            (void)sceneBuilder.addSceneObjectToRoot(pShape, pMaterial, std::move(pAreaLight));
        } else {
            (void)sceneBuilder.addSceneObjectToRoot(pShape, pMaterial);
        }
    }

    for (const auto* pPBFLight : pWorld->lightSources) {
        auto pLight = convertInfiniteLIght(pPBFLight);
        sceneBuilder.addInfiniteLight(std::move(pLight));
    }
    return sceneBuilder.build();
}

std::shared_ptr<pandora::Shape> Converter::convertShape(const PBFShape* pPBFShape)
{
    if (const auto* pPBFTriangleShape = dynamic_cast<const PBFTriangleMesh*>(pPBFShape)) {
        std::vector<glm::uvec3> indices { pPBFTriangleShape->index.size() };
        std::vector<glm::vec3> positions { pPBFTriangleShape->vertex.size() };
        std::vector<glm::vec3> normals { pPBFTriangleShape->normal.size() };
        std::vector<glm::vec2> texCoords;
        std::copy(
            std::begin(pPBFTriangleShape->index),
            std::end(pPBFTriangleShape->index),
            std::begin(indices));
        std::copy(
            std::begin(pPBFTriangleShape->vertex),
            std::end(pPBFTriangleShape->vertex),
            std::begin(positions));
        std::copy(
            std::begin(pPBFTriangleShape->normal),
            std::end(pPBFTriangleShape->normal),
            std::begin(normals));
        return std::make_shared<pandora::TriangleShape>(
            std::move(indices), std::move(positions), std::move(normals), std::move(texCoords));
    } else {
        spdlog::error("Unknown shape type encountered");
        return nullptr;
    }
}

std::shared_ptr<pandora::Material> Converter::convertMaterial(const PBFMaterial* pMaterial)
{
    if (auto iter = m_materialCache.find(pMaterial); iter != std::end(m_materialCache)) {
        return iter->second;
    } else {
        std::shared_ptr<pandora::Material> pPandoraMaterial { nullptr };
        if (const auto* pMatteMaterial = dynamic_cast<const PBFMatteMaterial*>(pMaterial)) {
            auto pKdTexture = getTexOptional(pMatteMaterial->mapKd, pMatteMaterial->kd);
            auto pSigmaTexture = getTexOptional(pMatteMaterial->mapSigma, pMatteMaterial->sigma);

            pPandoraMaterial = std::make_shared<pandora::MatteMaterial>(pKdTexture, pSigmaTexture);
        } else {
            spdlog::error("Unknown shape type encountered");
        }
        m_materialCache[pMaterial] = pPandoraMaterial;
        return pPandoraMaterial;
    }
}

std::unique_ptr<pandora::InfiniteLight> Converter::convertInfiniteLIght(const PBFLightSource* pLight)
{
    if (const auto* pInfiniteLightSource = dynamic_cast<const PBFInfiniteLightSource*>(pLight)) {
        auto pTexture = m_texCache.getImageTexture<glm::vec3>(pInfiniteLightSource->filePath);
        return std::make_unique<pandora::EnvironmentLight>(
            pInfiniteLightSource->transform, pInfiniteLightSource->L * pInfiniteLightSource->scale, pTexture);
    } else if (const auto* pDistantLightSource = dynamic_cast<const PBFDistantLightSource*>(pLight)) {
        const glm::vec3 L = pDistantLightSource->L * pDistantLightSource->scale;
        const glm::vec3 wLight = pDistantLightSource->to - pDistantLightSource->from;
        return std::make_unique<pandora::DistantLight>(
            glm::identity<glm::mat4>(), L, wLight);
    } else {
        spdlog::error("Unknown light type encountered");
        return nullptr;
    }
}

template <typename T>
std::shared_ptr<pandora::Texture<T>> Converter::getTexture(const PBFTexture* pTexture)
{
    if (const auto* pConstantTexture = dynamic_cast<const PBFConstantTexture*>(pTexture)) {
        if constexpr (std::is_same_v<T, float>)
            return m_texCache.getConstantTexture<T>(pConstantTexture->value.x);
        else
            return m_texCache.getConstantTexture<T>(pConstantTexture->value);
    } else if (const auto* pImageTexture = dynamic_cast<const PBFImageTexture*>(pTexture)) {
        return m_texCache.getImageTexture<T>(pImageTexture->filePath);
    } else {
        spdlog::error("Unknown texture type encountered");
        return nullptr;
    }
}

template <typename T>
std::shared_ptr<pandora::Texture<T>> Converter::getTexOptional(const PBFTexture* pTexture, T constValue)
{
    if (pTexture)
        return getTexture<T>(pTexture);
    else
        return m_texCache.getConstantTexture<T>(constValue);
}

}