#pragma once
#include "pandora/graphics_core/load_from_file.h"
#include "pbf/lexer.h"
#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <memory_resource>

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
    std::string name;
};
struct PBFMatteMaterial : public PBFMaterial {
    glm::vec3 kd { 0.5f };
    std::shared_ptr<PBFTexture> mapKd;

    float sigma { 0.0f };
    std::shared_ptr<PBFTexture> mapSigma;
    std::shared_ptr<PBFTexture> mapBump;
};

struct PBFAreaLight {
    glm::vec3 L;
};

struct PBFShape {
    PBFMaterial material;
    bool reverseOrientation { false };
    std::vector<std::tuple<std::string, std::shared_ptr<PBFTexture>>> textures;
    std::unique_ptr<PBFAreaLight> areaLight;
};

struct PBFLightSource {
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


struct PBFObject;
struct PBFInstance {
    std::shared_ptr<PBFObject> object;
    glm::mat4x3 transform;
};
struct PBFObject {
    std::vector<PBFShape> shapes;
    std::vector<PBFLightSource> lightSources;
    std::vector<PBFInstance> instances;

    std::string name;
};

struct PBFWorld {
    std::vector<std::unique_ptr<PBFShape>> shapes;
    std::vector<std::unique_ptr<PBFLightSource>> lightSources;
    std::vector<PBFInstance> instances;
    std::string name;
};

struct PBFFilm{
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

    PBFFilm* parseFilm();
    PBFCamera* parseCamera();
    PBFScene* parseScene();

	// Read pointer / pointers to existing entities
	template <typename T>
    T* getNextEntity();
    template <typename T>
    std::pmr::vector<T*> getNextEntityVector();

	template <typename T>
    T* allocate();

private:
    Lexer* m_pLexer { nullptr };
    std::pmr::monotonic_buffer_resource m_memoryResource;

	std::vector<void*> m_readEntities;
};

template <typename T>
inline T* Parser::getNextEntity()
{
    const int32_t id = m_pLexer->readT<int32_t>();

	if (id == -1)
        return nullptr;

	return reinterpret_cast<T*>(m_readEntities[id]);
}

template <typename T>
inline std::pmr::vector<T*> Parser::getNextEntityVector()
{
    const uint64_t length = m_pLexer->readT<uint64_t>();
    std::pmr::vector<T*> vt { length, &m_memoryResource };
    for (uint64_t i = 0; i < length; i++) {
        vt[i] = getNextEntity<T>();
    }
    return vt;
}

template <typename T>
inline T* Parser::allocate()
{
    void* pMem = m_memoryResource.allocate(sizeof(T), std::alignment_of_v<T>);
    return new (pMem) T();
}

}