#include "pandora/graphics_core/load_from_file.h"
#include "pandora/core/stats.h"
#include "pandora/flatbuffers/triangle_mesh_generated.h"
#include "pandora/graphics_core/load_from_file.h"
#include "pandora/graphics_core/perspective_camera.h"
#include "pandora/graphics_core/scene.h"
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

        const glm::mat4 cameraToWorldTransform = readMat4(cameraJson["camera_to_world_transform"]);
        const float aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
        //glm::vec4 position = cameraTransform * glm::vec4(0, 0, 0, 1);
        //std::cout << "Camera pos: [" << position.x << ", " << position.y << ", " << position.z << ", " << position.w << "]" << std::endl;
        config.camera = std::make_unique<PerspectiveCamera>(aspectRatio, cameraFovX, cameraToWorldTransform);
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
            for (const auto& jsonMaterial : sceneJson["materials"]) {
                auto materialType = jsonMaterial["type"].get<std::string>();
                auto arguments = jsonMaterial["arguments"];

                if (materialType == "matte") {
                    const auto& kd = getColorTexture(arguments["kd"].get<int>());
                    const auto& sigma = getFloatTexture(arguments["sigma"].get<int>());
                    materials.push_back(std::make_shared<MatteMaterial>(kd, sigma));
                } else {
                    spdlog::warn("Unknown material type {}! Substituting with placeholder", materialType);
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

        //assert(SUBDIVIDE_LEVEL == 1);
        spdlog::info("Loading geometry");
        std::vector<std::shared_ptr<Shape>> shapes;
        shapes.resize(sceneJson["shapes"].size());

        tbb::task_group tg;
        size_t i = 0;
        for (const auto& jsonGeometry : sceneJson["shapes"]) {
            auto geometryType = jsonGeometry["type"].get<std::string>();
            auto geometryFile = basePath / std::filesystem::path(jsonGeometry["filename"].get<std::string>());

            if (geometryFile.extension() == ".bin"s) {
                size_t startByte = jsonGeometry["start_byte"];
                size_t sizeBytes = jsonGeometry["size_bytes"];
                ALWAYS_ASSERT(std::filesystem::exists(geometryFile));
                auto mappedFile = mio::mmap_source(geometryFile.string(), startByte, sizeBytes);

                // NOTE: create a shared_ptr of mio::mmap_source and pass it to the lambdas instead of moving mio::mmap_source directly into
                //       the lambda as a work-around for a bug in TBB (C++ feature detection breaks when using clang-cl.exe which makes it
                //       think that move operators are not supported yet).
                if (jsonGeometry.find("transform") != jsonGeometry.end()) {
                    glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                    tg.run([i, &shapes, pMappedFile = std::make_shared<mio::mmap_source>(std::move(mappedFile)), transform]() {
                        shapes[i] = std::make_shared<TriangleShape>(
                            TriangleShape::loadSerialized(serialization::GetTriangleMesh(pMappedFile->data()), transform));
                    });
                } else {
                    tg.run([i, &shapes, pMappedFile = std::make_shared<mio::mmap_source>(std::move(mappedFile))]() {
                        shapes[i] = std::make_shared<TriangleShape>(
                            TriangleShape::loadSerialized(serialization::GetTriangleMesh(pMappedFile->data()), glm::mat4(1.0f)));
                    });
                }
            } else {
                glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                tg.run([i, &shapes, geometryFile, transform]() {
                    std::optional<TriangleShape> meshOpt = TriangleShape::loadFromFileSingleShape(geometryFile, transform);
                    ALWAYS_ASSERT(meshOpt.has_value());

                    shapes[i] = std::make_shared<TriangleShape>(std::move(*meshOpt));
                });
            }
            i++;
        }
        tg.wait();

        // Create scene objects
        spdlog::info("Creating scene objects");
        SceneBuilder sceneBuilder;
        std::vector<std::shared_ptr<SceneObject>> sceneObjects;
        for (const auto& jsonSceneObject : sceneJson["scene_objects"]) {
            auto pShape = shapes[jsonSceneObject["geometry_id"].get<int>()];
            std::shared_ptr<Material> pMaterial;
            if (loadMaterials)
                pMaterial = materials[jsonSceneObject["material_id"].get<int>()];
            else
                pMaterial = defaultMaterial;

            std::shared_ptr<SceneObject> pSceneObject;
            if (jsonSceneObject.find("area_light") != jsonSceneObject.end()) {
                auto areaLightRadiance = readVec3(jsonSceneObject["area_light"]["L"]);
                auto pAreaLight = std::make_unique<AreaLight>(areaLightRadiance);
                pSceneObject = sceneBuilder.addSceneObject(pShape, pMaterial, std::move(pAreaLight));
            } else {
                pSceneObject = sceneBuilder.addSceneObject(pShape, pMaterial);
            }
            sceneObjects.push_back(pSceneObject);
        }

        // Creating scene nodes
        spdlog::info("Creating scene nodes");
        const auto jsonSceneNodes = sceneJson["scene_nodes"];
        std::unordered_map<int, std::shared_ptr<SceneNode>> sceneNodeCache;
        std::function<std::shared_ptr<SceneNode>(int)> createSceneNodeRecurse = [&](int sceneNodeID) {
            if (auto iter = sceneNodeCache.find(sceneNodeID); iter != std::end(sceneNodeCache))
                return iter->second;

            auto jsonSceneNode = jsonSceneNodes[sceneNodeID];

            std::shared_ptr<SceneNode> pSceneNode = sceneBuilder.addSceneNode();

            for (const int sceneObjectID : jsonSceneNode["objects"])
                sceneBuilder.attachObject(pSceneNode, sceneObjects[sceneObjectID]);

            for (const auto& childLink : jsonSceneNode["children"]) {
                auto pChildNode = createSceneNodeRecurse(childLink["id"].get<int>());

                if (auto iter = childLink.find("transform"); iter != std::end(childLink)) {
                    sceneBuilder.attachNode(pSceneNode, pChildNode, getTransform(childLink["transform"]));
                } else {
                    sceneBuilder.attachNode(pSceneNode, pChildNode);
                }
            }

            sceneNodeCache[sceneNodeID] = pSceneNode;
            return pSceneNode;
        };
        auto pRootNode = createSceneNodeRecurse(sceneJson["root_scene_node"].get<int>());
        sceneBuilder.makeRootNode(pRootNode);

        // Load lights
        spdlog::info("Create infinite lights");
        for (const auto& jsonLight : sceneJson["lights"]) {
            std::string type = jsonLight["type"].get<std::string>();
            if (type == "infinite") {
                spdlog::debug("Creating environment area light");
                auto transform = getTransform(jsonLight["transform"]);
                auto scale = readVec3(jsonLight["scale"]);
#ifndef NDEBUG
                auto numSamples = jsonLight["num_samples"].get<int>();
                assert(numSamples == 1);
#endif
                auto texture = getColorTexture(jsonLight["texture"].get<int>());

                spdlog::debug("Environment area light with id: {}", jsonLight["texture"].get<int>());
                spdlog::info("Brightness scale: [{}, {}, {}]", scale.x, scale.y, scale.z);
                sceneBuilder.addInfiniteLight(std::make_unique<EnvironmentLight>(transform, scale, texture));
            } else if (type == "distant") {
                auto transform = getTransform(jsonLight["transform"]);
                auto L = readVec3(jsonLight["L"]);
                auto dir = readVec3(jsonLight["direction"]);

                spdlog::debug("Creating distant area light");
                sceneBuilder.addInfiniteLight(std::unique_ptr<DistantLight>(new DistantLight(transform, L, dir)));
            } else {
                THROW_ERROR("Unknown light type");
            }
        }

        config.pScene = std::make_unique<Scene>(sceneBuilder.build());
    }

    spdlog::info("Finished parsing scene");
    return config;
}
}
