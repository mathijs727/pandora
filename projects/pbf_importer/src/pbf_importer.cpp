#include "pbf/pbf_importer.h"
#include "pbf/lexer.h"
#include "pbf/parser.h"
#include "texture_cache.h"
#include <optional>
#include <pandora/graphics_core/perspective_camera.h>
#include <pandora/graphics_core/scene.h>
#include <pandora/lights/area_light.h>
#include <pandora/lights/distant_light.h>
#include <pandora/lights/environment_light.h>
#include <pandora/materials/matte_material.h>
#include <pandora/shapes/triangle.h>
#include <spdlog/spdlog.h>

namespace pbf {

pandora::RenderConfig pbfToRenderConfig(PBFScene* pScene, unsigned cameraID, tasking::CacheBuilder* pCacheBuilder, unsigned subdiv, bool loadTextures);

pandora::RenderConfig loadFromPBFFile(
    std::filesystem::path filePath, unsigned cameraID, tasking::CacheBuilder* pCacheBuilder, unsigned subdiv, bool loadTextures)
{
    Lexer lexer { filePath };
    Parser parser { &lexer };
    auto* pPBFScene = parser.parse();

    spdlog::info("Converting pbf scene into Pandora scene");
    return pbfToRenderConfig(pPBFScene, cameraID, pCacheBuilder, subdiv, loadTextures);
}

struct Converter {
    Converter(tasking::CacheBuilder* pCacheBuilder, bool loadTextures);

    static pandora::PerspectiveCamera convertCamera(const PBFCamera* pBbfCamera, float aspectRatio);
    pandora::Scene convertWorld(const PBFObject* pWorld, unsigned subdiv);
    void convertObject(
        const PBFObject* pObject,
        const std::shared_ptr<pandora::SceneNode>& pSceneNode,
        std::optional<glm::mat4> optTransform,
        pandora::SceneBuilder& sceneBuilder,
        unsigned subdiv);

    std::shared_ptr<pandora::Shape> convertShape(const PBFShape* pShape, unsigned subdiv);
    std::shared_ptr<pandora::Material> convertMaterial(const PBFMaterial* pMaterial);
    std::unique_ptr<pandora::InfiniteLight> convertInfiniteLight(const PBFLightSource* pLight);

    template <typename T>
    std::shared_ptr<pandora::Texture<T>> getTexture(const PBFTexture* pTexture);
    template <typename T>
    std::shared_ptr<pandora::Texture<T>> getTexOptional(const PBFTexture* pTexture, T constValue);

private:
    bool m_loadTextures;
    tasking::CacheBuilder* m_pCacheBuilder;

    TextureCache m_texCache;
    std::unordered_map<const PBFMaterial*, std::shared_ptr<pandora::Material>> m_materialCache;
    std::unordered_map<const PBFShape*, std::shared_ptr<pandora::Shape>> m_shapeCache;
    std::unordered_map<const PBFObject*, std::shared_ptr<pandora::SceneNode>> m_instanceCache;
};

pandora::RenderConfig pbfToRenderConfig(PBFScene* pScene, unsigned cameraID, tasking::CacheBuilder* pCacheBuilder, unsigned subdiv, bool loadTextures)
{
    const glm::ivec2 resolution = pScene->pFilm->resolution;
    const float aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);

    Converter converter { pCacheBuilder, loadTextures };

    pandora::RenderConfig out {};
    out.camera = std::make_unique<pandora::PerspectiveCamera>(Converter::convertCamera(pScene->cameras[cameraID], aspectRatio));
    out.resolution = resolution;
    out.pScene = std::make_unique<pandora::Scene>(converter.convertWorld(pScene->pWorld, subdiv));
    return out;
}

Converter::Converter(tasking::CacheBuilder* pCacheBuilder, bool loadTextures)
    : m_loadTextures(loadTextures)
    , m_pCacheBuilder(pCacheBuilder)
{
}

pandora::PerspectiveCamera Converter::convertCamera(const PBFCamera* pPbfCamera, float aspectRatio)
{
    // PBRT defines field of view along shortest axis
    float fovX = pPbfCamera->fov;
    if (aspectRatio > 1.0f)
        fovX = glm::degrees(std::atan(std::tan(glm::radians(fovX / 2.0f)) * aspectRatio) * 2.0f);

    const glm::mat4 transform = pPbfCamera->frame;
    const glm::mat4 frame = pPbfCamera->frame;
    const glm::vec3 center = frame * glm::vec4(0, 0, 0, 1);
    const float distance = glm::length(center - pPbfCamera->simplified.lenseCenter);
    if (distance > 0.001) {
        spdlog::error("Distance between frame and simplified: {}", distance);
        throw std::runtime_error("");
    }
    return pandora::PerspectiveCamera(aspectRatio, fovX, pPbfCamera->lensRadius, pPbfCamera->focalDistance, transform);
}

pandora::Scene Converter::convertWorld(const PBFObject* pWorld, unsigned subdiv)
{
    pandora::SceneBuilder sceneBuilder;
    convertObject(pWorld, nullptr, {}, sceneBuilder, subdiv);
    return sceneBuilder.build();
}

void Converter::convertObject(
    const PBFObject* pObject,
    const std::shared_ptr<pandora::SceneNode>& pSceneNode,
    std::optional<glm::mat4> optTransform,
    pandora::SceneBuilder& sceneBuilder,
    unsigned subdiv)
{
    for (const auto* pPBFShape : pObject->shapes) {
        if (!pPBFShape)
            continue;

        std::shared_ptr<pandora::Shape> pShape;
        if (auto iter = m_shapeCache.find(pPBFShape); iter != std::end(m_shapeCache)) {
            pShape = iter->second;
        } else {
            pShape = convertShape(pPBFShape, subdiv);
            m_shapeCache[pPBFShape] = pShape;
            m_pCacheBuilder->registerCacheable(pShape.get(), true);
        }

        auto pMaterial = convertMaterial(pPBFShape->pMaterial);

        // Load area light
        std::shared_ptr<pandora::SceneObject> pSceneObject;
        if (pPBFShape->pAreaLight) {
            auto pAreaLight = std::make_unique<pandora::AreaLight>(pPBFShape->pAreaLight->L);
            pSceneObject = sceneBuilder.addSceneObject(pShape, pMaterial, std::move(pAreaLight));
        } else {
            pSceneObject = sceneBuilder.addSceneObject(pShape, pMaterial);
        }

        if (pSceneNode) {
            sceneBuilder.attachObject(pSceneNode, pSceneObject);
        } else {
            sceneBuilder.attachObjectToRoot(pSceneObject);
        }
    }

    for (const auto* pPBFLight : pObject->lightSources) {
        auto pLight = convertInfiniteLight(pPBFLight);
        sceneBuilder.addInfiniteLight(std::move(pLight));
    }

    for (const auto* pPBFInstance : pObject->instances) {
        auto* pPBFChild = pPBFInstance->pObject;

        auto optChildTransform = optTransform;
        if (pPBFInstance->transform != glm::identity<glm::mat4x3>()) {
            if (!optChildTransform)
                optChildTransform = pPBFInstance->transform;
            else
                optChildTransform = *optChildTransform * glm::mat4(pPBFInstance->transform);
        }

        if (pPBFInstance->transform != glm::identity<glm::mat4x3>() || pObject->shapes.size() > 1) {
            std::shared_ptr<pandora::SceneNode> pChild;
            if (auto iter = m_instanceCache.find(pPBFChild); iter != std::end(m_instanceCache)) {
                pChild = iter->second;
            } else {
                // We add the child to our current node with it's full transformation, so the transformation of objects relative to the child node should be empty.
                pChild = sceneBuilder.addSceneNode();
                convertObject(pPBFChild, pChild, {}, sceneBuilder, subdiv);
                m_instanceCache[pPBFChild] = pChild;
            }

            if (optChildTransform) {
                if (pSceneNode)
                    sceneBuilder.attachNode(pSceneNode, pChild, *optChildTransform);
                else
                    sceneBuilder.attachNodeToRoot(pChild, *optChildTransform);
            } else {
                if (pSceneNode)
                    sceneBuilder.attachNode(pSceneNode, pChild);
                else
                    sceneBuilder.attachNodeToRoot(pChild);
            }
        } else {
            // Flatten nodes without transforms
            convertObject(pPBFChild, pSceneNode, optChildTransform, sceneBuilder, subdiv);
        }
    }
}

std::shared_ptr<pandora::Shape> Converter::convertShape(const PBFShape* pPBFShape, unsigned subdiv)
{
    if (const auto* pPBFTriangleShape = dynamic_cast<const PBFTriangleMesh*>(pPBFShape)) {
        std::vector<glm::uvec3> indices { static_cast<size_t>(pPBFTriangleShape->index.size()) };
        std::vector<glm::vec3> positions { static_cast<size_t>(pPBFTriangleShape->vertex.size()) };
        std::vector<glm::vec2> texCoords { static_cast<size_t>(pPBFTriangleShape->texCoord.size()) };
        std::vector<glm::vec3> normals { static_cast<size_t>(pPBFTriangleShape->normal.size()) };
        std::copy(
            std::begin(pPBFTriangleShape->index),
            std::end(pPBFTriangleShape->index),
            std::begin(indices));
        std::copy(
            std::begin(pPBFTriangleShape->vertex),
            std::end(pPBFTriangleShape->vertex),
            std::begin(positions));
        std::copy(
            std::begin(pPBFTriangleShape->texCoord),
            std::end(pPBFTriangleShape->texCoord),
            std::begin(texCoords));
        std::copy(
            std::begin(pPBFTriangleShape->normal),
            std::end(pPBFTriangleShape->normal),
            std::begin(normals));
        // TODO: support tex coords when subdividing triangle shape
        texCoords.clear();
        texCoords.shrink_to_fit();
        auto pShape = std::make_shared<pandora::TriangleShape>(
            std::move(indices), std::move(positions), std::move(normals), std::move(texCoords));

        for (unsigned i = 0; i < subdiv; i++)
            pShape->subdivide();

        return pShape;
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
            spdlog::error("Unknown material type encountered");
            static std::shared_ptr<pandora::Material> pDefaultMaterial { nullptr };
            if (!pDefaultMaterial) {
                auto pKdTexture = m_texCache.getConstantTexture(glm::vec3(0.5f));
                auto pSigmaTexture = m_texCache.getConstantTexture(0.0f);

                pDefaultMaterial = std::make_shared<pandora::MatteMaterial>(pKdTexture, pSigmaTexture);
            }
            pPandoraMaterial = pDefaultMaterial;
        }
        m_materialCache[pMaterial] = pPandoraMaterial;
        return pPandoraMaterial;
    }
}

std::unique_ptr<pandora::InfiniteLight> Converter::convertInfiniteLight(const PBFLightSource* pLight)
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
    } else if (!m_loadTextures) {
        if constexpr (std::is_same_v<T, float>)
            return m_texCache.getConstantTexture<T>(0.5f);
        else
            return m_texCache.getConstantTexture<T>(glm::vec3(0.5f));
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
