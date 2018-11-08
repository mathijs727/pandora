#include "pandora/core/load_from_file.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/distant_light.h"
#include "pandora/lights/environment_light.h"
#include "pandora/materials/matte_material.h"
#include "pandora/materials/translucent_material.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"
#include "pandora/textures/constant_texture.h"
#include "pandora/textures/image_texture.h"
#include "pandora/utility/error_handling.h"
#include "pandora/core/stats.h"
#include "pandora/eviction/lru_cache.h"
#include "pandora/eviction/fifo_cache.h"
#include <array>
#include <atomic>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <tbb/concurrent_vector.h>
#include <tbb/task_group.h>
#include <tuple>
#include <vector>
#include <mio/shared_mmap.hpp>

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

    std::cout << "PARSING JSON" << std::endl;

    // Read file and parse json
    nlohmann::json json;
    {
        std::ifstream file(filePath);
        if (!file.is_open())
            THROW_ERROR("Could not open scene file");

        file >> json;
    }

    std::cout << "Loading camera data from JSON" << std::endl;

    RenderConfig config;
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

    std::cout << "Loading scene from JSON" << std::endl;
    {
        auto sceneJson = json["scene"];
        auto dummyFloatTexture = std::make_shared<ConstantTexture<float>>(1.0f);
        auto dummyColorTexture = std::make_shared<ConstantTexture<glm::vec3>>(glm::vec3(1.0f));

        std::cout << "Making map of textures" << std::endl;
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
                    std::cout << "Unknown texture class \"" << textureClass << "\"! Substituting with placeholder..." << std::endl;
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
                    std::cout << "Loading imagemap from file " << arguments["filename"].get<std::string>() << " for texture id " << texID << std::endl;
                    auto textureFile = basePath / std::filesystem::path(arguments["filename"].get<std::string>());
                    _colorTextures[texID] = std::make_shared<ImageTexture<glm::vec3>>(textureFile);
                } else {
                    std::cout << "Unknown texture class \"" << textureClass << "\"! Substituting with placeholder..." << std::endl;
                    _colorTextures[texID] = dummyColorTexture;
                }
            }

            return _colorTextures[texID];
        };

        std::cout << "Collecting list of materials" << std::endl;
        // Load materials
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

        std::cout << "Loading geometry" << std::endl;
        std::vector<std::shared_ptr<TriangleMesh>> geometry;
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
                    std::optional<TriangleMesh> meshOpt = TriangleMesh(
                        serialization::GetTriangleMesh(mappedFile.data()), transform);
                    ALWAYS_ASSERT(meshOpt.has_value());
                    geometry.push_back(std::make_shared<TriangleMesh>(std::move(*meshOpt)));
                } else {
                    std::optional<TriangleMesh> meshOpt = TriangleMesh(
                        serialization::GetTriangleMesh(mappedFile.data()));
                    ALWAYS_ASSERT(meshOpt.has_value());
                    geometry.push_back(std::make_shared<TriangleMesh>(std::move(*meshOpt)));
                }
            } else {
                glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                std::optional<TriangleMesh> meshOpt = TriangleMesh::loadFromFileSingleMesh(geometryFile, transform);
                ALWAYS_ASSERT(meshOpt.has_value());
                geometry.push_back(std::make_shared<TriangleMesh>(std::move(*meshOpt)));
            }
        }

        auto makeGeomSceneObject = [&](nlohmann::json jsonSceneObject) {
            auto mesh = geometry[jsonSceneObject["geometry_id"].get<int>()];
            std::shared_ptr<Material> material;
            if (loadMaterials)
                material = materials[jsonSceneObject["material_id"].get<int>()];
            else
                material = defaultMaterial;

            if (jsonSceneObject.find("area_light") != jsonSceneObject.end()) {
                auto areaLight = readVec3(jsonSceneObject["area_light"]["L"]);
                return std::make_unique<InCoreGeometricSceneObject>(mesh, material, areaLight);
            } else {
                return std::make_unique<InCoreGeometricSceneObject>(mesh, material);
            }
        };

        std::cout << "Creating instance base scene objects" << std::endl;
        // Create instanced base objects
        std::vector<std::shared_ptr<InCoreGeometricSceneObject>> baseSceneObjects;
        for (const auto jsonSceneObject : sceneJson["instance_base_scene_objects"]) {
            baseSceneObjects.emplace_back(makeGeomSceneObject(jsonSceneObject)); // Converts to shared_ptr
        }

        std::cout << "Creating final scene objects" << std::endl;
        // Create scene objects
        for (const auto jsonSceneObject : sceneJson["scene_objects"]) {
            if (jsonSceneObject["instancing"].get<bool>()) {
                glm::mat4 transform = getTransform(jsonSceneObject["transform"]);
                auto baseSceneObject = baseSceneObjects[jsonSceneObject["base_scene_object_id"].get<int>()];
                auto instancedSceneObject = std::make_unique<InCoreInstancedSceneObject>(transform, baseSceneObject);
                config.scene.addSceneObject(std::move(instancedSceneObject));
            } else {
                config.scene.addSceneObject(makeGeomSceneObject(jsonSceneObject));
            }
        }

        // Load lights
        for (const auto jsonLight : sceneJson["lights"]) {
            std::string type = jsonLight["type"].get<std::string>();
            if (type == "infinite") {
                std::cout << "Infinite area light" << std::endl;
                auto transform = getTransform(jsonLight["transform"]);
                auto scale = readVec3(jsonLight["scale"]);
                auto numSamples = jsonLight["num_samples"].get<int>();
                auto texture = getColorTexture(jsonLight["texture"].get<int>());

                std::cout << "Infinite area light with id: " << jsonLight["texture"].get<int>() << std::endl;

                config.scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, scale, numSamples, texture));
            } else if (type == "distant") {
                auto transform = getTransform(jsonLight["transform"]);
                auto L = readVec3(jsonLight["L"]);
                auto dir = readVec3(jsonLight["direction"]);

                std::cout << "Distant area light" << std::endl;
                config.scene.addInfiniteLight(std::make_shared<DistantLight>(transform, L, dir));
            } else {
                THROW_ERROR("Unknown light type");
            }
        }
    }

    std::cout << "DONE PARSING SCENE" << std::endl;
    return std::move(config);
}

RenderConfig loadFromFileOOC(std::filesystem::path filePath, bool loadMaterials)
{
    auto basePath = filePath.parent_path();

    // Delete the out-of-core acceleration structure cache folder if the previous render was
    // of a different scene.
    {
        auto oocNodeCacheFolder = std::filesystem::path("./ooc_node_cache/");
        auto cacheVersionFilePath = "./version.txt";
        if (std::filesystem::exists(oocNodeCacheFolder)) {

            std::ifstream cacheFile(cacheVersionFilePath);
            std::string oldVersion((std::istreambuf_iterator<char>(cacheFile)), std::istreambuf_iterator<char>());

            if (oldVersion != filePath) {
                std::filesystem::remove_all(oocNodeCacheFolder);
            }
        }

        std::ofstream cacheFile(cacheVersionFilePath);
        cacheFile << filePath.string();
    }

    // Read file and parse json
    nlohmann::json json;
    {
        std::cout << "Loading & parsing JSON" << std::endl;
        std::ifstream file(filePath);
        if (!file.is_open())
            THROW_ERROR("Could not open scene file");

        file >> json;
    }

    // Argument is the size of the geometry cache. This is just a temporary cache because the batched
    // acceleration structure creates its own for rendering (clearing this cache).
    RenderConfig config(64llu * 1024llu * 1024llu * 1024llu);
    {
        std::cout << "Getting config" << std::endl;
        auto configJson = json["config"];

        auto cameraJson = configJson["camera"];
        auto cameraType = cameraJson["type"].get<std::string>();
        ALWAYS_ASSERT(cameraType == "perspective");

        auto resolutionJson = cameraJson["resolution"];
        glm::ivec2 resolution = { resolutionJson[0].get<int>(), resolutionJson[1].get<int>() };
        std::cout << "Resolution: (" << resolution[0] << ", " << resolution[1] << ")" << std::endl;
        float cameraFov = cameraJson["fov"].get<float>();

        // FOV in defined along the narrower of the image's width and height
        float cameraFovX;
        if (resolution.x < resolution.y) {
            cameraFovX = cameraFov;
        } else {
            float aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
            cameraFovX = glm::degrees(std::atan(aspectRatio * std::tan(glm::radians(cameraFov / 2.0f)))) * 2.0f;
        }

        glm::mat4 cameraToWorldTransform = readMat4(cameraJson["camera_to_world_transform"]);
        config.camera = std::make_unique<PerspectiveCamera>(resolution, cameraFovX, cameraToWorldTransform);
        config.resolution = resolution;
    }

    {
        std::cout << "Loading scene" << std::endl;
        auto sceneJson = json["scene"];
        auto dummyFloatTexture = std::make_shared<ConstantTexture<float>>(1.0f);
        auto dummyColorTexture = std::make_shared<ConstantTexture<glm::vec3>>(glm::vec3(1.0f));

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
                    std::cout << "Unknown texture class \"" << textureClass << "\"! Substituting with placeholder..." << std::endl;
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
                    auto textureFile = basePath / std::filesystem::path(arguments["filename"].get<std::string>());
                    _colorTextures[texID] = std::make_shared<ImageTexture<glm::vec3>>(textureFile);
                } else {
                    std::cout << "Unknown texture class \"" << textureClass << "\"! Substituting with placeholder..." << std::endl;
                    _colorTextures[texID] = dummyColorTexture;
                }
            }

            return _colorTextures[texID];
        };

        // Load materials
        std::cout << "Loading materials" << std::endl;
        std::vector<std::shared_ptr<Material>> materials;
        if (loadMaterials) {
            std::cout << "Loading materials" << std::endl;
            for (const auto jsonMaterial : sceneJson["materials"]) {
                auto materialType = jsonMaterial["type"].get<std::string>();
                auto arguments = jsonMaterial["arguments"];

                if (materialType == "matte") {
                    const auto& kd = getColorTexture(arguments["kd"].get<int>());
                    const auto& sigma = getFloatTexture(arguments["sigma"].get<int>());
                    materials.push_back(std::make_shared<MatteMaterial>(kd, sigma));
                } else if(materialType == "translucent") {
                    auto kd = getColorTexture(arguments["kd"].get<int>());
                    auto ks = getColorTexture(arguments["ks"].get<int>());
                    auto roughness = getFloatTexture(arguments["roughness"].get<int>());
                    auto transmit = getColorTexture(arguments["transmit"].get<int>());
                    auto reflect = getColorTexture(arguments["reflect"].get<int>());
                    materials.push_back(std::make_shared<TranslucentMaterial>(kd, ks, roughness, reflect, transmit, true));
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

        std::cout << "Loading geometry" << std::endl;
        std::unordered_map<std::string, mio::shared_mmap_source> mappedGeometryFiles;
        auto* geometryCache = config.scene.geometryCache();
        std::vector<EvictableResourceID> geometry;
        for (const auto jsonGeometry : sceneJson["geometry"]) {
            auto geometryType = jsonGeometry["type"].get<std::string>();
            auto geometryFile = basePath / std::filesystem::path(jsonGeometry["filename"].get<std::string>());

            if (geometryFile.extension() == ".bin"s) {
                size_t startByte = jsonGeometry["start_byte"];
                size_t sizeBytes = jsonGeometry["size_bytes"];

                auto goemetryFileString = geometryFile.string();
                if (mappedGeometryFiles.find(goemetryFileString) == mappedGeometryFiles.end()) {
                    mappedGeometryFiles[goemetryFileString] = mio::shared_mmap_source(geometryFile.string(), 0, mio::map_entire_file);
                }
                auto mappedGeometryFile = mappedGeometryFiles[goemetryFileString];

                EvictableResourceID resourceID;
                if (jsonGeometry.find("transform") != jsonGeometry.end()) {
                    glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                    resourceID = geometryCache->emplaceFactoryUnsafe<TriangleMesh>([=]() -> TriangleMesh {
                        auto buffer = gsl::make_span(mappedGeometryFile.data(), mappedGeometryFile.size()).subspan(startByte, sizeBytes);
                        std::optional<TriangleMesh> meshOpt = TriangleMesh(serialization::GetTriangleMesh(buffer.data()), transform);
                        ALWAYS_ASSERT(meshOpt.has_value());
                        return std::move(*meshOpt);
                    });
                } else {
                    resourceID = geometryCache->emplaceFactoryUnsafe<TriangleMesh>([=]() -> TriangleMesh {
                        auto buffer = gsl::make_span(mappedGeometryFile.data(), mappedGeometryFile.size()).subspan(startByte, sizeBytes);
                        std::optional<TriangleMesh> meshOpt = TriangleMesh(serialization::GetTriangleMesh(buffer.data()));
                        ALWAYS_ASSERT(meshOpt.has_value());
                        return std::move(*meshOpt);
                    });
                }
                
                geometry.push_back(resourceID);
            } else {
                glm::mat4 transform = getTransform(jsonGeometry["transform"]);
                auto resourceID = geometryCache->emplaceFactoryUnsafe<TriangleMesh>([=]() -> TriangleMesh {
                    std::optional<TriangleMesh> meshOpt = TriangleMesh::loadFromFileSingleMesh(geometryFile, transform);
                    ALWAYS_ASSERT(meshOpt.has_value());
                    return std::move(*meshOpt);
                });
                geometry.push_back(resourceID);
            }
        }

        // Note returns a factory function. This ensures that the json isn't touched from multiple threads which
        // might not be thread-safe:
        // https://github.com/nlohmann/json/issues/800
        auto geomSceneObjectFactoryFunc = [&](nlohmann::json jsonSceneObject) -> std::function<std::unique_ptr<OOCGeometricSceneObject>(void)> {
            auto geometryResourceID = geometry[jsonSceneObject["geometry_id"].get<int>()];
            auto geometry = EvictableResourceHandle<TriangleMesh, CacheT<TriangleMesh>>(geometryCache, geometryResourceID);

            std::shared_ptr<Material> material;
            if (loadMaterials)
                material = materials[jsonSceneObject["material_id"].get<int>()];
            else
                material = defaultMaterial;

            if (jsonSceneObject.find("area_light") != jsonSceneObject.end()) {
                glm::vec3 lightEmitted = readVec3(jsonSceneObject["area_light"]["L"]);

                return [=]() {
                    return std::make_unique<OOCGeometricSceneObject>(geometry, material, lightEmitted);
                };
            } else {
                return [=]() {
                    return std::make_unique<OOCGeometricSceneObject>(geometry, material);
                };
            }
        };

        // Create instanced base objects
        std::cout << "Creating instanced base objects (multi-threaded)" << std::endl;
        auto jsonBaseSceneObjects = sceneJson["instance_base_scene_objects"];
        size_t numBaseSceneObjects = jsonBaseSceneObjects.size();
        tbb::task_group taskGroup;
        std::vector<std::shared_ptr<OOCGeometricSceneObject>> baseSceneObjects(numBaseSceneObjects);
        for (size_t i = 0; i < numBaseSceneObjects; i++) {
            auto geomSceneObjectFactory = geomSceneObjectFactoryFunc(jsonBaseSceneObjects[i]);
            taskGroup.run([i, geomSceneObjectFactory, &baseSceneObjects]() {
                auto sceneObject = geomSceneObjectFactory();
                ALWAYS_ASSERT(sceneObject != nullptr);
                baseSceneObjects[i] = std::move(sceneObject); // Converts to shared_ptr
            });
        }
        std::cout << "Wait till completion..." << std::endl;
        taskGroup.wait();

        std::cout << "Geometry loaded: " << static_cast<size_t>(g_stats.memory.geometryLoaded) / 1000000 << std::endl;
        std::cout << "Geometry evicted: " << static_cast<size_t>(g_stats.memory.geometryEvicted) / 1000000<< std::endl;

        // Create scene objects
        // NOTE: create in parallel but make sure to add them to the scene in a fixed order. This ensures that the
        //       area lights are added in the same order, thus light sampling is deterministic between runs.
        std::cout << "Creating final objects (multi-threaded)" << std::endl;
        auto jsonSceneObjects = sceneJson["scene_objects"];
        size_t numSceneObjects = jsonSceneObjects.size();
        std::vector<std::unique_ptr<OOCSceneObject>> sceneObjects(numSceneObjects);
        std::cout << "Preallocated (empty) scene object pointer array of size: " << numSceneObjects << std::endl;
        for (size_t i = 0; i < numSceneObjects; i++) {
            const auto jsonSceneObject = jsonSceneObjects[i];
            if (jsonSceneObject["instancing"].get<bool>()) {
                glm::mat4 transform = getTransform(jsonSceneObject["transform"]);
                auto baseSceneObject = baseSceneObjects[jsonSceneObject["base_scene_object_id"].get<int>()];
                auto instancedSceneObject = std::make_unique<OOCInstancedSceneObject>(transform, baseSceneObject);
                sceneObjects[i] = std::move(instancedSceneObject);
            } else {
                auto sceneObjectFactory = geomSceneObjectFactoryFunc(jsonSceneObject);
                taskGroup.run([&sceneObjects, i, sceneObjectFactory]() {
                    sceneObjects[i] = sceneObjectFactory();
                });
            }
        }

        std::cout << "Wait till completion..." << std::endl;
        taskGroup.wait();
        std::cout << "Done loading, adding to scene" << std::endl;
        for (auto& sceneObject : sceneObjects) {
            if (sceneObject) {
                config.scene.addSceneObject(std::move(sceneObject));
            }
        }

        // Load lights
        std::cout << "Loading lights" << std::endl;
        for (const auto jsonLight : sceneJson["lights"]) {
            std::string type = jsonLight["type"].get<std::string>();
            if (type == "infinite") {
                auto transform = getTransform(jsonLight["transform"]);
                auto scale = readVec3(jsonLight["scale"]);
                auto numSamples = jsonLight["num_samples"].get<int>();
                auto texture = getColorTexture(jsonLight["texture"].get<int>());

                config.scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, scale, numSamples, texture));
            } else if (type == "distant") {
                auto transform = getTransform(jsonLight["transform"]);
                auto L = readVec3(jsonLight["L"]);
                auto dir = readVec3(jsonLight["direction"]);

                config.scene.addInfiniteLight(std::make_shared<DistantLight>(transform, L, dir));
            } else {
                THROW_ERROR("Unknown light type");
            }
        }
    }

    return std::move(config);
}

}
