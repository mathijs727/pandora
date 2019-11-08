#pragma once
#include "pandora/graphics_core/load_from_file.h"
#include "pbf/lexer.h"
#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <memory_resource>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace pbf {

struct PBFTexture {
};
struct PBFConstantTexture : public PBFTexture {
    glm::vec3 value;
};
struct PBFImageTexture : public PBFTexture {
    std::filesystem::path filePath;
};

struct PBFMaterial {
    std::string_view name;
};
struct PBFMatteMaterial : public PBFMaterial {
    glm::vec3 kd { 0.5f };
    PBFTexture* mapKd;

    float sigma { 0.0f };
    PBFTexture* mapSigma;
    PBFTexture* mapBump;
};

struct PBFLightSource {
};
struct PBFDiffuseAreaLight { // : public PBFLightSource
    glm::vec3 L;
};
struct PBFInfiniteLightSource : public PBFLightSource {
    std::filesystem::path filePath;
    glm::mat4x3 transform;
    glm::vec3 L { 1.0f };
    glm::vec3 scale { 1.0f };
    int nSamples { 1 };
};
struct PBFDistantLightSource : public PBFLightSource {
    glm::vec3 from { 0 };
    glm::vec3 to { 0, 0, 1 };
    glm::vec3 L { 1 };
    glm::vec3 scale { 1.0f };
};

struct PBFShape {
    PBFMaterial* pMaterial;
    bool reverseOrientation { false };
    std::pmr::unordered_map<std::string_view, PBFTexture*> textures;
    PBFDiffuseAreaLight* pAreaLight { nullptr };
};
struct PBFTriangleMesh : public PBFShape {
    std::pmr::vector<glm::ivec3> index;
    std::pmr::vector<glm::vec3> vertex;
    std::pmr::vector<glm::vec3> normal;
    //std::pmr::vector<glm::vec2> texCoord;
};

struct PBFObject;
struct PBFInstance {
    PBFObject* pObject;
    glm::mat4x3 transform;
};
struct PBFObject {
    std::pmr::vector<PBFShape*> shapes;
    std::pmr::vector<PBFLightSource*> lightSources;
    std::pmr::vector<PBFInstance*> instances;

    std::string_view name;
};

struct PBFWorld {
    std::vector<std::unique_ptr<PBFShape>> shapes;
    std::vector<std::unique_ptr<PBFLightSource>> lightSources;
    std::vector<PBFInstance> instances;
    std::string name;
};

struct PBFFilm {
    glm::ivec2 resolution;
    std::filesystem::path filePath;
};

struct PBFCamera {
    float fov;
    float focalDistance;
    float lensRadius;
    glm::mat4x3 frame;

    struct Simplified {
        glm::vec3 screenCenter;
        glm::vec3 screenU;
        glm::vec3 screenV;
        glm::vec3 lenseCenter;
        glm::vec3 lenseDu;
        glm::vec3 lenseDv;
    };
    Simplified simplified;
};

struct PBFScene {
    std::pmr::vector<PBFCamera*> cameras;
    PBFFilm* pFilm;
    PBFWorld* pWorld;
};

class Parser {
public:
    Parser(Lexer* pLexer);

    pandora::RenderConfig parse();

private:
    void* parseEntity();

    PBFScene* parseScene();
    PBFObject* parseObject();
    PBFInstance* parseInstance();
    PBFCamera* parseCamera();
    PBFFilm* parseFilm();

    void parseMaterial(PBFMaterial* pOut);
    PBFMatteMaterial* parseMatteMaterial();

    void parseTexture(PBFTexture* pOut);
    PBFImageTexture* parseImageTexture();
    PBFConstantTexture* parseConstantTexture();

    void parseShape(PBFShape* pOut);
    PBFTriangleMesh* parseTriangleMesh();

	void parseDiffuseAreaLight(PBFDiffuseAreaLight* pLight);
    PBFDiffuseAreaLight* parseDiffuseAreaLightBB();
    PBFDiffuseAreaLight* parseDiffuseAreaLightRGB();
    PBFInfiniteLightSource* parseInfiniteLightSource();
    PBFDistantLightSource* parseDistantLightSource();

    // Read pointer / pointers to existing entities
    template <typename T>
    T read();
    template <typename T>
    std::pmr::vector<T> readVector(); // Vector with uint64_t length
    template <typename T>
    std::pmr::vector<T> readShortVector(); // Vector with int32_t length
    template <typename T1, typename T2>
    std::pmr::unordered_map<T1, T2> readMap();

    template <typename T>
    T* allocate();

private:
    Lexer* m_pLexer { nullptr };
    std::pmr::monotonic_buffer_resource m_memoryResource;

    std::vector<void*> m_readEntities;
};

template <typename T>
inline T Parser::read()
{
    if constexpr (std::is_pointer_v<T>) {
        // Entity pointer
        const int32_t id = m_pLexer->readT<int32_t>();

        if (id == -1)
            return nullptr;

        return reinterpret_cast<T>(m_readEntities[id]);
    } else {
        return m_pLexer->readT<T>();
    }
}

template <typename T>
inline std::pmr::vector<T> Parser::readVector()
{
    const uint64_t length = m_pLexer->readT<uint64_t>();
    std::pmr::vector<T> vt { length, &m_memoryResource };
    for (uint64_t i = 0; i < length; i++) {
        vt[i] = read<T>();
    }
    return vt;
}

template <typename T>
inline std::pmr::vector<T> Parser::readShortVector()
{
    const int32_t length = m_pLexer->readT<int32_t>();
    std::pmr::vector<T> vt { static_cast<size_t>(length), &m_memoryResource };
    for (int32_t i = 0; i < length; i++) {
        vt[i] = read<T>();
    }
    return vt;
}

template <typename T1, typename T2>
inline std::pmr::unordered_map<T1, T2> Parser::readMap()
{
    const uint32_t size = m_pLexer->readT<uint32_t>();

    std::pmr::unordered_map<T1, T2> mt { 2 * size, &m_memoryResource };
    for (uint64_t i = 0; i < size; i++) {
        T1 t1 = read<T1>();
        T2 t2 = read<T2>();
        mt[t1] = t2;
    }
    return mt;
}

template <typename T>
inline T* Parser::allocate()
{
    void* pMem = m_memoryResource.allocate(sizeof(T), std::alignment_of_v<T>);
    return new (pMem) T();
}

}