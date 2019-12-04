#pragma once
#include "pandora/graphics_core/load_from_file.h"
#include "pbf/entities.h"
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

class Parser {
public:
    Parser(Lexer* pLexer);
    ~Parser();

    PBFScene* parse();

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
    gsl::span<const T> readSpan(); // Vector with uint64_t length
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
    PBFScene* m_pScene;
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
inline gsl::span<const T> Parser::readSpan()
{
    const uint64_t length = m_pLexer->readT<uint64_t>();
    return m_pLexer->readArray<T>(length);
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