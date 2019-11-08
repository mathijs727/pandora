#include "pbf/parser.h"
#include <spdlog/spdlog.h>

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

pandora::RenderConfig Parser::parse()
{
    PBFScene scene;

    while (!m_pLexer->endOfFile()) {
        m_readEntities.push_back(parseEntity());
    }

    return pandora::RenderConfig {};
}

void* Parser::parseEntity()
{
    const uint64_t size = m_pLexer->readT<uint64_t>();
    const int32_t tag = m_pLexer->readT<uint32_t>();

    switch (tag) {
    case Type::SCENE:
        return parseScene();
    case Type::CAMERA:
        return parseCamera();
    case Type::FILM:
        return parseFilm();
    default: {
        spdlog::warn("Skipping unsupported entity type with tag {}", tag);
        (void)m_pLexer->readArray<std::byte>(size);
        return nullptr;
    } break;
    }
}

PBFFilm* Parser::parseFilm()
{
    PBFFilm* pOut = allocate<PBFFilm>();
    pOut->resolution = m_pLexer->readT<glm::ivec2>();
    pOut->filePath = m_pLexer->readT<std::filesystem::path>();
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

PBFScene* Parser::parseScene()
{
    PBFScene* pOut = allocate<PBFScene>();
    pOut->pFilm = getNextEntity<PBFFilm>();
    pOut->cameras = getNextEntityVector<PBFCamera>();
    pOut->pWorld = getNextEntity<PBFWorld>();
    return pOut;
}

}