#include "pandora/graphics_core/load_from_file.h"
#include "pandora/graphics_core/load_from_file.h"
#include "pandora/core/stats.h"
#include "pandora/flatbuffers/triangle_mesh_generated.h"
#include "pandora/lights/distant_light.h"
#include "pandora/lights/environment_light.h"
#include "pandora/materials/matte_material.h"
#include "pandora/materials/translucent_material.h"
#include "pandora/shapes/triangle.h"
#include "pandora/textures/constant_texture.h"
#include "pandora/textures/image_texture.h"
#include "pandora/utility/error_handling.h"
#include <array>
#include <atomic>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <mio/shared_mmap.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <tbb/concurrent_vector.h>
#include <tbb/task_group.h>
#include <tuple>
#include <vector>

using namespace std::string_literals;

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

RenderConfig loadFromFile(std::filesystem::path filePath, bool loadMaterials)
{
    auto basePath = filePath.parent_path();

    spdlog::info("Parsing {}", filePath.string());

    // Read file and parse json
    nlohmann::json json;
    {
        std::ifstream file(filePath);
        if (!file.is_open())
            THROW_ERROR("Could not open scene file");

        file >> json;
    }

    RenderConfig config;

    spdlog::info("Loading camera data");
    {
        auto configJson = json["config"];

        auto cameraJson = configJson["camera"];
        auto cameraType = cameraJson["type"].get<std::string>();
        ALWAYS_ASSERT(cameraType == "perspective");

        auto resolutionJson = cameraJson["resolution"];
        glm::ivec2 resolution = { resolutionJson[0].get<int>(), resolutionJson[1].get<int>() };
        const float cameraFov = cameraJson["fov"].get<float>();
        // FOV in defined along the narrower of the image's width and height
        float cameraFovX;
        if (resolution.x < resolution.y) {
            cameraFovX = cameraFov;
        } else {
            float aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
            cameraFovX = glm::degrees(std::atan(aspectRatio * std::tan(glm::radians(cameraFov / 2.0f)))) * 2.0f;
        }

        glm::mat4 cameraToWorldTransform = readMat4(cameraJson["camera_to_world_transform"]);
        //glm::vec4 position = cameraTransform * glm::vec4(0, 0, 0, 1);
        //std::cout << "Camera pos: [" << position.x << ", " << position.y << ", " << position.z << ", " << position.w << "]" << std::endl;
        config.camera = std::make_unique<PerspectiveCamera>(resolution, cameraFovX, cameraToWorldTransform);
        config.resolution = resolution;
    }

    {
        auto sceneJson = json["scene"];
        auto dummyFloatTexture = std::make_shared<ConstantTexture<float>>(1.0f);
        auto dummyColorTexture = std::make_shared<ConstantTexture<glm::vec3>>(glm::vec3(1.0f));

        spdlog::info("Creating texture lookup table");
        // Lazy load textures to reduce memory usage when loadMaterials is false
        std::unordered_map<int, std::shared_ptr<Texture<float>>> _floatTextures;
        auto getFloatTexture = [&](int texID) {
            if (_floatTextures.find(texID) == _floatTextures.end()) {
                const auto jsonFloatTexture = sceneJson["float_textures"][texID];
                auto textureClass = jsonFloatTexture["class"].get<std::string>();
                auto arguments = jsonFloatTexture["arguments"];

                if (textureClass == "constant") {
                    float value = arguments["value"].get<float>();
                    _floatTextures[texID] = std::make_shared<ConstantTexture<float>>(value);
                } else if (textureClass == "imagemap") {
                    auto textureFile = basePath / std::filesystem::path(arguments["filename"].get<std::string>());
                    _floatTextures[texID] = std::make_shared<ImageTexture<float>>(textureFile);
                } else {
                    spdlog::warn("Unknown texture class {}! Substituting with a placeholder texture", textureClass);
                    _floatTextures[texID] = dummyFloatTexture;
                }
            }

            return _floatTextures[texID];
        };

        std::unordered_map<int, std::shared_ptr<Texture<glm::vec3>>> _colorTextures;
        auto getColorTexture = [&](int texID) {
            if (_colorTextures.find(texID) == _colorTextures.end()) {
                const auto jsonColorTexture = sceneJson["color_textures"][texID];
                auto textureClass = jsonColorTexture["class"].get<std::string>();
                auto arguments = jsonColorTexture["arguments"];

                if (textureClass == "constant") {
                    glm::vec3 value = readVec3(arguments["value"]);
                    _colorTextures[texID] = std::make_shared<ConstantTexture<glm::vec3>>(value);
                } else if (textureClass == "imagemap") {
                    spdlog::info("Loading imagemap from file {} for texture id {}", arguments["filename"].get<std::string>(), texID);
                    auto textureFile = basePath / std::filesystem::path(arguments["filename"].get<std::string>());
                    _colorTextures[texID] = std::make_shared<ImageTexture<glm::vec3>>(textureFile);
                } else {
                    spdlog::warn("Unknown texture class {}! Substituting with a placeholder texture", textureClass);
                    _colorTextures[texID] = dummyColorTexture;
                }
            }

            return _colorTextures[texID];
        };

        // Load materials
        spdlog::info("Loading materials");
        std::vector<std::shared_ptr<Material>> materials;
        if (loadMaterials) {
            for (const auto jsonMaterial : sceneJson["materials"]) {
                auto materialType = jsonMaterial["type"].get<std::string>();
                auto arguments = jsonMaterial["arguments"];

                if (materialType == "matte") {
                    const auto& kd = getColorTexture(arguments["kd"].get<int>());
                    const auto& sigma = getFloatTexture(arguments["sigma"].get<int>());
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

        // Load geometry
        auto getTransform = [&](int id) {
            return readMat4(sceneJson["transforms"][id]);
        };

        assert(SUBDIVIDE_LEVEL == 1);
        spdlog::info("Loading geometry");
        std::vector<std::shared_ptr<Shape>> geometry;
        for (const auto jsonGeometry : sceneJson["geometry"]) {
            auto geometryType = jsonGeometry["type"].get<std::string>();
            auto geometryFile = basePath / std::filesystem::path(jsonGeometry["filename"].get<std::string>());

            if (geometryFile.extension() == ".bin"s) {
                size_t startByte = jsonGeometry["start_byte"];
                size_t sizeBytes = jsonGeometry["size_bytes"];
                ALWAYS_ASSERT(std::filesystem::exists(geometryFile));
                auto mappedFile = mio::mmap_source(geometryFile.string(), startByte, sizeBytes);

                if (jsonGeometry.find("transform") != jsonGeometry.end()) {
                    glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                    auto pShape = std::make_shared<TriangleShape>(
                        TriangleShape::loadSerialized(serialization::GetTriangleMesh(mappedFile.data()), transform));
                    /*for (int subDiv = 0; subDiv < SUBDIVIDE_LEVEL; subDiv++) {
                        meshOpt->subdivide();
                    }*/
                    geometry.push_back(pShape);
                } else {
                    auto pShape = std::make_shared<TriangleShape>(
                        TriangleShape::loadSerialized(serialization::GetTriangleMesh(mappedFile.data()), glm::mat4(1.0f)));
                    /*for (int subDiv = 0; subDiv < SUBDIVIDE_LEVEL; subDiv++) {
                        meshOpt->subdivide();
                    }*/
                    geometry.push_back(pShape);
                }
            } else {
                glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                std::optional<TriangleShape> meshOpt = TriangleShape::loadFromFileSingleShape(geometryFile, transform);
                ALWAYS_ASSERT(meshOpt.has_value());

                /*for (int subDiv = 0; subDiv < SUBDIVIDE_LEVEL; subDiv++) {
                    meshOpt->subdivide();
                }*/

                geometry.push_back(std::make_shared<TriangleShape>(std::move(*meshOpt)));
            }
        }

        /*auto createSceneObject = [&](nlohmann::json jsonSceneObject) -> std::unique_ptr<SceneObject> {
            auto mesh = geometry[jsonSceneObject["geometry_id"].get<int>()];
            std::shared_ptr<Material> material;
            if (loadMaterials)
                material = materials[jsonSceneObject["material_id"].get<int>()];
            else
                material = defaultMaterial;

            if (jsonSceneObject.find("area_light") != jsonSceneObject.end()) {
                auto areaLight = readVec3(jsonSceneObject["area_light"]["L"]);
                return std::make_unique<InCoreGeometricSceneObject>(mesh, material, areaLight);
                //std::cout << "Skipping area light scene object for debugging" << std::endl;
                //return nullptr;
            } else {
                return std::make_unique<InCoreGeometricSceneObject>(mesh, material);
            }
        };

        std::cout << "Creating instance base scene objects" << std::endl;
        // Create instanced base objects
        std::vector<std::shared_ptr<InCoreGeometricSceneObject>> baseSceneObjects;
        for (const auto jsonSceneObject : sceneJson["instance_base_scene_objects"]) {
            auto sceneObject = makeGeomSceneObject(jsonSceneObject);
            g_stats.scene.uniquePrimitives += sceneObject->numPrimitives();
            baseSceneObjects.emplace_back(std::move(sceneObject)); // Converts to shared_ptr
        }*/

        // Create scene objects
        spdlog::info("Creating scene objects");
        SceneBuilder sceneBuilder;
        for (const auto jsonSceneObject : sceneJson["scene_objects"]) {
            if (jsonSceneObject["instancing"].get<bool>()) {
                spdlog::warn("Instancing is not yet supported, ignoring instanced scene object");
                /*glm::mat4 transform = getTransform(jsonSceneObject["transform"]);
                auto baseSceneObject = baseSceneObjects[jsonSceneObject["base_scene_object_id"].get<int>()];
                auto instancedSceneObject = std::make_unique<InCoreInstancedSceneObject>(transform, baseSceneObject);

                if (instancedSceneObject) {
                    g_stats.scene.totalPrimitives += instancedSceneObject->numPrimitives();

                    instancedSceneObject->sceneObjectID = sceneObjectID++;
                    config.scene.addSceneObject(std::move(instancedSceneObject));
                }*/
            } else {
                auto pShape = geometry[jsonSceneObject["geometry_id"].get<int>()];
                std::shared_ptr<Material> pMaterial;
                if (loadMaterials)
                    pMaterial = materials[jsonSceneObject["material_id"].get<int>()];
                else
                    pMaterial = defaultMaterial;

                if (jsonSceneObject.find("area_light") != jsonSceneObject.end()) {
                    auto areaLightRadiance = readVec3(jsonSceneObject["area_light"]["L"]);
                    auto pAreaLight = std::make_unique<AreaLight>(areaLightRadiance, pShape.get());
                    sceneBuilder.addSceneObject(pShape, pMaterial, std::move(pAreaLight));
                } else {
                    sceneBuilder.addSceneObject(pShape, pMaterial);
                }

                g_stats.scene.uniquePrimitives += pShape->numPrimitives();
                g_stats.scene.totalPrimitives += pShape->numPrimitives();
            }
        }

        // Load lights
        spdlog::info("Create infinite lights");
        for (const auto jsonLight : sceneJson["lights"]) {
            spdlog::info("Infinite light source not supported yet");
            /*std::string type = jsonLight["type"].get<std::string>();
            if (type == "infinite") {
                std::cout << "Infinite area light" << std::endl;
                auto transform = getTransform(jsonLight["transform"]);
                auto scale = readVec3(jsonLight["scale"]);
                auto numSamples = jsonLight["num_samples"].get<int>();
                auto texture = getColorTexture(jsonLight["texture"].get<int>());

                //std::cout << "Skipping loading infinite area light for debugging" << std::endl;
                std::cout << "Infinite area light with id: " << jsonLight["texture"].get<int>() << std::endl;
                std::cout << "Brightness scale: [" << scale.x << ", " << scale.y << ", " << scale.z << "]" << std::endl;
                config.scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, scale, numSamples, texture));
            } else if (type == "distant") {
                auto transform = getTransform(jsonLight["transform"]);
                auto L = readVec3(jsonLight["L"]);
                auto dir = readVec3(jsonLight["direction"]);

                std::cout << "Distant area light" << std::endl;
                config.scene.addInfiniteLight(std::make_shared<DistantLight>(transform, L, dir));
            } else {
                THROW_ERROR("Unknown light type");
            }*/
        }
    }

    spdlog::info("Finished parsing scene");
    return std::move(config);
}

}
