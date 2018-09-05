#include "pandora/core/load_from_file.h"
#include "pandora/geometry/triangle.h"
#include "pandora/materials/matte_material.h"
#include "pandora/textures/constant_texture.h"
#include "pandora/textures/image_texture.h"
#include "pandora/utility/error_handling.h"
#include <array>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <tuple>
#include <vector>

namespace pandora {

static glm::mat4 readMat4(nlohmann::json json)
{
    std::array<std::array<float, 4>, 4> t;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++)
            t[j][i] = json[i][j].get<float>(); // NOTE: read transposed
    }

    // clang-format off
    glm::mat4 transform{
        t[0][0], t[0][1], t[0][2], t[0][3],
        t[1][0], t[1][1], t[1][2], t[1][3],
        t[2][0], t[2][1], t[2][2], t[2][3],
        t[3][0], t[3][1], t[3][2], t[3][3],
    };
    // clang-format on

    return transform;
}

static glm::vec3 readVec3(nlohmann::json json)
{
    return glm::vec3 { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() };
}

RenderConfig loadFromFile(std::string_view filename, bool loadMaterials)
{
    // Read file and parse json
    nlohmann::json json;
    {
        std::ifstream file(filename.data());
        if (!file.is_open())
            THROW_ERROR("Could not open scene file");

        file >> json;
    }

    RenderConfig config;
    {
        auto configJson = json["config"];

        auto cameraJson = configJson["camera"];
        auto cameraType = cameraJson["type"].get<std::string>();
        ALWAYS_ASSERT(cameraType == "perspective");

        auto resolutionJson = cameraJson["resolution"];
        glm::ivec2 resolution = { resolutionJson[0].get<int>(), resolutionJson[1].get<int>() };
        float cameraFov = cameraJson["fov"].get<float>();

        glm::mat4 cameraToWorldTransform = readMat4(cameraJson["camera_to_world_transform"]);
        //glm::vec4 position = cameraTransform * glm::vec4(0, 0, 0, 1);
        //std::cout << "Camera pos: [" << position.x << ", " << position.y << ", " << position.z << ", " << position.w << "]" << std::endl;
        config.camera = std::make_unique<PerspectiveCamera>(resolution, cameraFov, cameraToWorldTransform);
        config.resolution = resolution;
    }

    {
        auto sceneJson = json["scene"];

        auto dummyFloatTexture = std::make_shared<ConstantTexture<float>>(1.0f);
        auto dummyColorTexture = std::make_shared<ConstantTexture<glm::vec3>>(glm::vec3(1.0f));

        std::vector<std::shared_ptr<Material>> materials;
        if (loadMaterials) {
            std::vector<std::shared_ptr<Texture<float>>> floatTextures;
            for (const auto jsonFloatTexture : sceneJson["float_textures"]) {
                auto textureClass = jsonFloatTexture["class"].get<std::string>();
                auto arguments = jsonFloatTexture["arguments"];

                if (textureClass == "constant") {
                    float value = arguments["value"].get<float>();
                    floatTextures.push_back(std::make_shared<ConstantTexture<float>>(value));
                } else if (textureClass == "imagemap") {
                    auto filename = arguments["filename"].get<std::string>();
                    floatTextures.push_back(std::make_shared<ImageTexture<float>>(filename));
                } else {
                    std::cout << "Unknown texture class \"" << textureClass << "\"! Substituting with placeholder..." << std::endl;
                    floatTextures.push_back(dummyFloatTexture);
                }
            }

            std::vector<std::shared_ptr<Texture<glm::vec3>>> colorTextures;
            for (const auto jsonColorTexture : sceneJson["color_textures"]) {
                auto textureClass = jsonColorTexture["class"].get<std::string>();
                auto arguments = jsonColorTexture["arguments"];

                if (textureClass == "constant") {
                    glm::vec3 value = readVec3(arguments["value"]);
                    colorTextures.push_back(std::make_shared<ConstantTexture<glm::vec3>>(value));
                } else if (textureClass == "imagemap") {
                    auto filename = arguments["filename"].get<std::string>();
                    colorTextures.push_back(std::make_shared<ImageTexture<glm::vec3>>(filename));
                } else {
                    std::cout << "Unknown texture class \"" << textureClass << "\"! Substituting with placeholder..." << std::endl;
                    colorTextures.push_back(dummyColorTexture);
                }
            }

            for (const auto jsonMaterial : sceneJson["materials"]) {
                auto materialType = jsonMaterial["type"].get<std::string>();
                auto arguments = jsonMaterial["arguments"];

                if (materialType == "matte") {
                    const auto& kd = colorTextures[arguments["kd"].get<int>()];
                    const auto& sigma = floatTextures[arguments["sigma"].get<int>()];
                    materials.push_back(std::make_shared<MatteMaterial>(kd, sigma));
                } else {
                    std::cout << "Unknown material type \"" << materialType << "\"! Substituting with placeholder." << std::endl;
                    materials.push_back(std::make_shared<MatteMaterial>(dummyColorTexture, dummyFloatTexture));
                }
            }
        }

        static std::shared_ptr<Material> defaultMaterial = std::make_shared<MatteMaterial>(
            std::make_shared<ConstantTexture<glm::vec3>>(glm::vec3(1.0f)),
            std::make_shared<ConstantTexture<float>>(1.0f));
        std::vector<std::shared_ptr<TriangleMesh>> geometry;
        for (const auto jsonGeometry : sceneJson["geometry"]) {
            auto geometryType = jsonGeometry["type"].get<std::string>();
            auto filename = jsonGeometry["filename"].get<std::string>();

            glm::mat4 transform = readMat4(jsonGeometry["transform"]);
            auto meshes = TriangleMesh::loadFromFile(filename, transform);
            ALWAYS_ASSERT(meshes.size() == 1);
            geometry.push_back(std::make_shared<TriangleMesh>(std::move(meshes[0])));
        }

        for (const auto jsonSceneObject : sceneJson["scene_objects"]) {
            auto mesh = geometry[jsonSceneObject["geometry_id"].get<int>()];
            std::shared_ptr<Material> material;
            if (loadMaterials)
                material = materials[jsonSceneObject["material_id"].get<int>()];
            else
                material = defaultMaterial;
            config.scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
        }
    }

    return std::move(config);
}
}
