#include "pbf/parser.h"
#include <cmath>
#include <spdlog/spdlog.h>

static glm::vec3 colorTempToRGB(float colorTemp);

// Binary file format lexer for PBF format by Ingo Wald.
// Based on code by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/semantic/BinaryFileFormat.cpp

static constexpr int32_t supportedFormatTag = 5;

enum Type : uint32_t {
    ERR = 0,
    SCENE,
    OBJECT,
    SHAPE,
    INSTANCE,
    CAMERA,
    FILM,
    SPECTRUM,

    MATERIAL = 10,
    DISNEY_MATERIAL,
    UBER_MATERIAL,
    MIX_MATERIAL,
    GLASS_MATERIAL,
    MIRROR_MATERIAL,
    MATTE_MATERIAL,
    SUBSTRATE_MATERIAL,
    SUBSURFACE_MATERIAL,
    FOURIER_MATERIAL,
    METAL_MATERIAL,
    PLASTIC_MATERIAL,
    TRANSLUCENT_MATERIAL,

    TEXTURE = 30,
    IMAGE_TEXTURE,
    SCALE_TEXTURE,
    PTEX_FILE_TEXTURE,
    CONSTANT_TEXTURE,
    CHECKER_TEXTURE,
    WINDY_TEXTURE,
    FBM_TEXTURE,
    MARBLE_TEXTURE,
    MIX_TEXTURE,
    WRINKLED_TEXTURE,

    TRIANGLE_MESH = 50,
    QUAD_MESH,
    SPHERE,
    DISK,
    CURVE,

    DIFFUSE_AREA_LIGHT_BB = 60,
    DIFFUSE_AREA_LIGHT_RGB,

    INFINITE_LIGHT_SOURCE = 70,
    DISTANT_LIGHT_SOURCE
};

namespace pbf {

Parser::Parser(Lexer* pLexer)
    : m_pLexer(pLexer)
    , m_memoryResource(std::pmr::new_delete_resource())
{
    const int32_t formatTag = m_pLexer->readT<int32_t>();
    if (formatTag != supportedFormatTag) {
        spdlog::warn("pbf file uses a different format tag ({}) than what this library is expecting ({})", formatTag, supportedFormatTag);

        int32_t supportedMajor = supportedFormatTag >> 16;
        int32_t fileMajor = formatTag >> 16;
        if (supportedMajor != fileMajor) {
            spdlog::error("Even the *major* file format version is different - "
                          "this means the file _should_ be incompatible with this library. "
                          "Please regenreate the pbf file.");
            throw std::runtime_error("");
        }
    }
}

PBFScene* Parser::parse()
{
    while (!m_pLexer->endOfFile()) {
        m_readEntities.push_back(parseEntity());
    }

	return m_pScene;
}

void* Parser::parseEntity()
{
    const uint64_t size = m_pLexer->readT<uint64_t>();
    const int32_t tag = m_pLexer->readT<uint32_t>();

    switch (tag) {
    case Type::SCENE: {
        auto* pScene = parseScene();
        m_pScene = pScene;
        return pScene;
    } break;
    case Type::OBJECT:
        return parseObject();
    case Type::INSTANCE:
        return parseInstance();
    case Type::CAMERA:
        return parseCamera();
    case Type::FILM:
        return parseFilm();
    case Type::MATTE_MATERIAL:
        return parseMatteMaterial();
    case Type::IMAGE_TEXTURE:
        return parseImageTexture();
    case Type::CONSTANT_TEXTURE:
        return parseConstantTexture();
    case Type::TRIANGLE_MESH:
        return parseTriangleMesh();
    case Type::DIFFUSE_AREA_LIGHT_BB:
        return parseDiffuseAreaLightBB();
    case Type::DIFFUSE_AREA_LIGHT_RGB:
        return parseDiffuseAreaLightRGB();
    case INFINITE_LIGHT_SOURCE:
        return parseInfiniteLightSource();
    case DISTANT_LIGHT_SOURCE:
        return parseDistantLightSource();
    default: {
        spdlog::warn("Skipping unsupported entity type with tag {}", tag);
        (void)m_pLexer->readArray<std::byte>(size);
        return nullptr;
    } break;
    }
}

PBFScene* Parser::parseScene()
{
    PBFScene* pOut = allocate<PBFScene>();
    pOut->pFilm = read<PBFFilm*>();
    pOut->cameras = readVector<PBFCamera*>();
    pOut->pWorld = read<PBFObject*>();
    return pOut;
}

PBFObject* Parser::parseObject()
{
    PBFObject* pOut = allocate<PBFObject>();
    pOut->name = read<std::string_view>();
    pOut->shapes = readShortVector<PBFShape*>();
    pOut->lightSources = readShortVector<PBFLightSource*>();
    pOut->instances = readShortVector<PBFInstance*>();
    return pOut;
}

PBFInstance* Parser::parseInstance()
{
    PBFInstance* pOut = allocate<PBFInstance>();
    pOut->transform = read<glm::mat4x3>();
    pOut->pObject = read<PBFObject*>();
    return pOut;
}

PBFCamera* Parser::parseCamera()
{
    PBFCamera* pOut = allocate<PBFCamera>();
    pOut->fov = m_pLexer->readT<float>();
    pOut->focalDistance = m_pLexer->readT<float>();
    pOut->lensRadius = m_pLexer->readT<float>();
    pOut->frame = m_pLexer->readT<glm::mat4x3>();
    pOut->simplified = m_pLexer->readT<PBFCamera::Simplified>();
    return pOut;
}

PBFFilm* Parser::parseFilm()
{
    PBFFilm* pOut = allocate<PBFFilm>();
    pOut->resolution = m_pLexer->readT<glm::ivec2>();
    pOut->filePath = m_pLexer->readT<std::filesystem::path>();
    return pOut;
}

void Parser::parseMaterial(PBFMaterial* pOut)
{
    pOut->name = read<std::string_view>();
}

PBFMatteMaterial* Parser::parseMatteMaterial()
{
    PBFMatteMaterial* pOut = allocate<PBFMatteMaterial>();
    parseMaterial(pOut);
    pOut->mapKd = read<PBFTexture*>();
    pOut->kd = read<glm::vec3>();
    pOut->sigma = read<float>();
    pOut->mapSigma = read<PBFTexture*>();
    pOut->mapBump = read<PBFTexture*>();
    return pOut;
}

void Parser::parseTexture(PBFTexture* pOut)
{
    void;
}

PBFImageTexture* Parser::parseImageTexture()
{
    PBFImageTexture* pOut = allocate<PBFImageTexture>();
    parseTexture(pOut);
    pOut->filePath = read<std::filesystem::path>();
    return pOut;
}

PBFConstantTexture* Parser::parseConstantTexture()
{
    PBFConstantTexture* pOut = allocate<PBFConstantTexture>();
    pOut->value = read<glm::vec3>();
    return pOut;
}

void Parser::parseShape(PBFShape* pOut)
{
    pOut->pMaterial = read<PBFMaterial*>();
    pOut->textures = readMap<std::string_view, PBFTexture*>();
    pOut->pAreaLight = read<PBFDiffuseAreaLight*>();
    pOut->reverseOrientation = read<int8_t>();
}

PBFTriangleMesh* Parser::parseTriangleMesh()
{
    PBFTriangleMesh* pOut = allocate<PBFTriangleMesh>();
    parseShape(pOut);
    pOut->vertex = readVector<glm::vec3>();
    pOut->normal = readVector<glm::vec3>();
    pOut->index = readVector<glm::ivec3>();
    return pOut;
}

PBFDiffuseAreaLight* Parser::parseDiffuseAreaLightBB()
{
    const float temperature = read<float>();
    const float scale = read<float>();
    const glm::vec3 color = colorTempToRGB(temperature);

    PBFDiffuseAreaLight* pOut = allocate<PBFDiffuseAreaLight>();
    pOut->L = color * scale;
    return pOut;
}

PBFDiffuseAreaLight* Parser::parseDiffuseAreaLightRGB()
{
    PBFDiffuseAreaLight* pOut = allocate<PBFDiffuseAreaLight>();
    pOut->L = read<glm::vec3>();
    return pOut;
}

PBFInfiniteLightSource* Parser::parseInfiniteLightSource()
{
    PBFInfiniteLightSource* pOut = allocate<PBFInfiniteLightSource>();
    pOut->filePath = read<std::filesystem::path>();
    pOut->transform = read<glm::mat4x3>();
    pOut->L = read<glm::vec3>();
    pOut->scale = read<glm::vec3>();
    pOut->nSamples = read<int>();
    return pOut;
}

PBFDistantLightSource* Parser::parseDistantLightSource()
{
    PBFDistantLightSource* pOut = allocate<PBFDistantLightSource>();
    pOut->from = read<glm::vec3>();
    pOut->to = read<glm::vec3>();
    pOut->L = read<glm::vec3>();
    pOut->scale = read<glm::vec3>();
    return pOut;
}

}

// Probably not the most accurate (PBRT is probably better) but this should get the job done.
// http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
glm::vec3 colorTempToRGB(float colorTemp)
{
    glm::vec3 rgb { 0 };
    colorTemp /= 100.0f;

    // Red
    if (colorTemp <= 66) {
        rgb.r = 255;
    } else {
        rgb.r = 329.698727446f * std::powf(colorTemp - 60.0f, -0.1332047592f);
        rgb.r = std::clamp(rgb.r, 0.0f, 255.0f);
    }

    // Green
    if (colorTemp <= 66) {
        rgb.g = 99.4708025861f * std::logf(rgb.g) - 161.1195681661f;
    } else {
        rgb.g = 288.1221695283f * std::powf(colorTemp - 60, -0.0755148492f);
    }
    rgb.g = std::clamp(rgb.g, 0.0f, 255.0f);

    // Blue
    if (colorTemp >= 66) {
        rgb.b = 255;
    } else if (colorTemp <= 19) {
        rgb.b = 0;
    } else {
        rgb.b = 138.5177312231f * std::logf(colorTemp - 10) - 305.0447927307f;
        rgb.b = std::clamp(rgb.b, 0.0f, 255.0f);
    }

    return rgb / 255.0f;
}
