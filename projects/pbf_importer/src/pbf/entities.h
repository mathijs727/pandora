#pragma once

namespace pbf {

struct PBFTexture {
    virtual void dummy() const {}; // Must have virtual function to use dynamic_cast
};
struct PBFConstantTexture : public PBFTexture {
    glm::vec3 value;
};
struct PBFImageTexture : public PBFTexture {
    std::filesystem::path filePath;
};

struct PBFMaterial {
    std::string_view name;

    virtual void dummy() const {}; // Must have virtual function to use dynamic_cast
};
struct PBFMatteMaterial : public PBFMaterial {
    glm::vec3 kd { 0.5f };
    PBFTexture* mapKd;

    float sigma { 0.0f };
    PBFTexture* mapSigma;
    PBFTexture* mapBump;
};

struct PBFLightSource {
    virtual void dummy() const {}; // Must have virtual function to use dynamic_cast
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

struct PBFDiffuseAreaLight { // : public PBFLightSource
    glm::vec3 L;
};

struct PBFShape {
    PBFMaterial* pMaterial;
    bool reverseOrientation { false };
    std::pmr::unordered_map<std::string_view, PBFTexture*> textures;
    PBFDiffuseAreaLight* pAreaLight { nullptr };

    virtual void dummy() const {}; // Must have virtual function to use dynamic_cast
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
    PBFObject* pWorld;
};

}