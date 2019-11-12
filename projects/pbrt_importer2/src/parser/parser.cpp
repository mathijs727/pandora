#include "pbrt/parser/parser.h"
#include <glm/gtc/matrix_transform.hpp>
#include <pandora/lights/area_light.h>
#include <pandora/lights/distant_light.h>
#include <pandora/lights/environment_light.h>
#include <pandora/materials/matte_material.h>
#include <pandora/shapes/triangle.h>
#include <pandora/textures/constant_texture.h>
#include <pandora/textures/image_texture.h>
#include <spdlog/spdlog.h>
#include <tuple>

std::pair<std::string_view, std::string_view> splitStringFirstWhitespace(std::string_view string);

Parser::Parser(std::filesystem::path basePath, bool loadTextures)
    : m_loadTextures(loadTextures)
    , m_basePath(basePath)
{
}

pandora::RenderConfig Parser::parse(std::filesystem::path file, unsigned cameraID, tasking::CacheBuilder* pGeometryCacheBuilder)
{
    // Set a default material
    m_graphicsState.pMaterial = std::make_shared<pandora::MatteMaterial>(
        m_textureCache.getConstantTexture(glm::vec3(0.0f)),
        m_textureCache.getConstantTexture(0.0f));

    addLexer(file);

    PBRTIntermediateScene intermediateScene;
    intermediateScene.pGeometryCacheBuilder = pGeometryCacheBuilder;
    parseScene(intermediateScene);
    m_asyncWorkTaskGroup.wait();

    const glm::vec2 fResolution = intermediateScene.resolution;
    const float aspectRatio = fResolution.x / fResolution.y;
    std::vector<pandora::PerspectiveCamera> cameras;
    std::transform(std::begin(intermediateScene.cameras), std::end(intermediateScene.cameras), std::back_inserter(cameras),
        [=](auto cameraData) {
            // Use mutable reference instead of immutable reference to work around a Clang on Windows bug:
            // https://stackoverflow.com/questions/53721714/why-does-structured-binding-not-work-as-expected-on-struct
            auto& [worldToCamera, cameraParams] = cameraData;

            // PBRT defines field of view along shortest axis
            float fov = cameraParams.template get<float>("fov", 45.0f);
            if (fResolution.x > fResolution.y)
                fov = glm::degrees(std::atan(std::tan(glm::radians(fov / 2.0f)) * aspectRatio) * 2.0f);

            return pandora::PerspectiveCamera(aspectRatio, fov, glm::inverse(worldToCamera));
        });

    auto pScene = std::make_unique<pandora::Scene>(intermediateScene.sceneBuilder.build());
    auto pCamera = std::make_unique<pandora::PerspectiveCamera>(cameras[cameraID]);
    return pandora::RenderConfig { std::move(pScene), std::move(pCamera), intermediateScene.resolution };
}

void Parser::parseWorld(PBRTIntermediateScene& scene)
{
    while (true) {
        Token token = next();
        assert(token);
        assert(token.type == TokenType::LITERAL);

        // Make a copy of the lexer source so it doesn't immediately get deleted when
        // an include statement is encountered. This is a trade-off so we can keep working
        // with std::string_view's instead of making copies.
        auto pLexerSource = m_pCurrentLexerSource;
        (void)pLexerSource;

        if (token == "WorldEnd")
            break;

        if (parseTransform(token))
            continue;

        if (token == "AttributeBegin") {
            pushAttributes();
            continue;
        }
        if (token == "AttributeEnd") {
            popAttributes();
            continue;
        }

        if (token == "LightSource") {
            parseLightSource(scene);
            continue;
        }
        if (token == "AreaLightSource") {
            std::string_view lightType = next().text;
            auto params = parseParams();
            if (lightType == "diffuse") {
                m_graphicsState.emittedAreaLight = params.get<glm::vec3>("L", glm::vec3(1));
            } else {
                spdlog::warn("Ignoring area light of unsupported type \"{}\"", lightType);
            }
            continue;
        }
        if (token == "Material") {
            const Token typeToken = next();
            auto pMaterial = parseMaterial(typeToken);
            m_graphicsState.pMaterial = pMaterial;
            continue;
        }
        if (token == "MakeNamedMaterial") {
            const std::string_view name = next().text;
            auto pMaterial = parseMaterial(Token {});
            m_graphicsState.namedMaterials[std::string(name)] = pMaterial;
            continue;
        }
        if (token == "NamedMaterial") {
            const std::string_view name = next().text;
            m_graphicsState.pMaterial = m_graphicsState.namedMaterials.find(std::string(name))->second;
            continue;
        }
        if (token == "Texture") {
            parseTexture();
            continue;
        }
        if (token == "Shape") {
            parseShape(scene);
            continue;
        }
        if (token == "ObjectBegin") {
            if (m_currentObject) {
                m_objectStack.push(std::move(*m_currentObject));
                m_currentObject.reset();
            }

            const std::string_view name = next().text;
            m_currentObject = Object { std::string(name), scene.sceneBuilder.addSceneNode() };
            continue;
        }
        if (token == "ObjectEnd") {
            assert(m_currentObject);
            m_objects[m_currentObject->name] = std::move(m_currentObject->pSceneNode);

            if (!m_objectStack.empty()) {
                m_currentObject = m_objectStack.top();
                m_objectStack.pop();
            } else {
                m_currentObject.reset();
            }
            continue;
        }
        if (token == "ObjectInstance") {
            const std::string_view name = next().text;
            if (m_objects.find(std::string(name)) == std::end(m_objects)) {
                spdlog::error("ObjectInstance {} not found", name);
                continue;
            }
            auto pSceneNode = m_objects.find(std::string(name))->second;
            if (m_currentObject) {
                if (m_currentTransform == glm::identity<glm::mat4>())
                    scene.sceneBuilder.attachNode(m_currentObject->pSceneNode, pSceneNode);
                else
                    scene.sceneBuilder.attachNode(m_currentObject->pSceneNode, pSceneNode, m_currentTransform);
            } else {
                if (m_currentTransform == glm::identity<glm::mat4>()) {
                    scene.sceneBuilder.attachNodeToRoot(pSceneNode);
                } else {
                    scene.sceneBuilder.attachNodeToRoot(pSceneNode, m_currentTransform);
                }
            }
            continue;
        }

        spdlog::error("Unknown token {}", token.text);
    }
}

void Parser::parseScene(PBRTIntermediateScene& scene)
{
    while (peek()) {
        Token token = next();
        if (!token)
            break;

        // Make a copy of the lexer source so it doesn't immediately get deleted when
        // an include statement is encountered. This is a trade-off so we can keep working
        // with std::string_view's instead of making copies.
        auto pLexerSource = m_pCurrentLexerSource;
        (void)pLexerSource;

        if (token.type != TokenType::LITERAL) {
            spdlog::error("Unexpected non-literal token type \"{}\" with text \"{}\"", token.type, token.text);
        }

        if (parseTransform(token))
            continue;

        if (token == "Camera") {
            std::string_view cameraType = next().text;
            auto params = parseParams();
            if (cameraType != "perspective") {
                spdlog::error("Unsupported camera type \"{}\"", cameraType);
                continue;
            }

            scene.cameras.push_back({ m_currentTransform, std::move(params) });
            continue;
        }
        if (token == "Sampler") {
            (void)next(); // Sampler type
            (void)parseParams();
            spdlog::warn("Advanced samplers are not supported in Pandora");
            continue;
        }
        if (token == "Integrator") {
            (void)next(); // Integrator type
            (void)parseParams();
            spdlog::warn("Ignoring Integrator command");
            continue;
        }
        if (token == "Film") {
            (void)next(); // Film type (always "image")

            auto params = parseParams();

            const int resolutionX = params.get<int>("xresolution", 640);
            const int resolutionY = params.get<int>("yresolution", 480);
            scene.resolution = glm::ivec2(resolutionX, resolutionY);
            continue;
        }
        if (token == "WorldBegin") {
            // Reset CTM
            m_currentTransform = glm::identity<glm::mat4>();
            while (!m_transformStack.empty())
                m_transformStack.pop();
            m_transformStartActive = true;

            parseWorld(scene);
            continue;
        }

        spdlog::error("Unknown token {}", token.text);
    }
}

void Parser::parseLightSource(PBRTIntermediateScene& scene)
{
    std::string_view lightType = next().text;
    auto params = parseParams();
    if (lightType == "distant") {
        const glm::vec3 pointFrom = params.get<glm::vec3>("from", glm::vec3(0));
        const glm::vec3 pointTo = params.get<glm::vec3>("to", glm::vec3(0, 0, 1));
        const glm::vec3 direction = glm::normalize(pointTo - pointFrom);
        glm::vec3 L = params.get<glm::vec3>("L", glm::vec3(1));

        auto pLight = std::make_unique<pandora::DistantLight>(
            m_currentTransform, L, direction);
        scene.sceneBuilder.addInfiniteLight(std::move(pLight));
    } else if (lightType == "infinite") {
        const glm::vec3 L = params.get<glm::vec3>("L", glm::vec3(1));

        std::shared_ptr<pandora::Texture<glm::vec3>> pTexture;
        if (params.contains("mapname") && m_loadTextures) {
            pTexture = m_textureCache.getImageTexture<glm::vec3>(m_basePath / params.get<std::string_view>("mapname"));
        } else {
            pTexture = m_textureCache.getConstantTexture(glm::vec3(1.0f));
        }

        auto pLight = std::make_unique<pandora::EnvironmentLight>(m_currentTransform, L, pTexture);
        scene.sceneBuilder.addInfiniteLight(std::move(pLight));
    } else {
        spdlog::warn("Ignoring light of unsupported type \"{}\"", lightType);
    }
}

void Parser::parseShape(PBRTIntermediateScene& scene)
{
    std::string_view shapeType = next().text;
    auto params = parseParams();

    if (shapeType == "plymesh") {
        parsePlymesh(scene, std::move(params));
    } else if (shapeType == "trianglemesh") {
        parseTriangleShape(scene, std::move(params));
    } else {
        //spdlog::warn("Ignoring shape of unsupported type \"{}\"", shapeType);
    }
}

void Parser::parseTriangleShape(PBRTIntermediateScene& scene, Params&& params)
{
    // Create scene object
    std::shared_ptr<pandora::SceneObject> pSceneObject;
    if (m_graphicsState.emittedAreaLight) {
        auto pAreaLight = std::make_unique<pandora::AreaLight>(*m_graphicsState.emittedAreaLight);
        pSceneObject = scene.sceneBuilder.addSceneObject(nullptr, m_graphicsState.pMaterial, std::move(pAreaLight));
        pAreaLight.reset();
    } else {
        pSceneObject = scene.sceneBuilder.addSceneObject(nullptr, m_graphicsState.pMaterial);
    }

    // Add to scene if not an instance template
    if (!m_currentObject) {
        scene.sceneBuilder.attachObjectToRoot(pSceneObject);
    } else {
        scene.sceneBuilder.attachObject(m_currentObject->pSceneNode, pSceneObject);
    }

    m_asyncWorkTaskGroup.run([transform = m_currentTransform, pSceneObject, params = std::move(params), pLexerSource = m_pCurrentLexerSource, &scene]() {
        Params& mutParams = const_cast<Params&>(params);

        // Load mesh
        std::vector<int> integerIndices = mutParams.getMove<std::vector<int>>("indices");
        assert(integerIndices.size() % 3 == 0);
        std::vector<glm::uvec3> indices;
        indices.resize(integerIndices.size() / 3);
        for (size_t i = 0, i3 = 0; i < indices.size(); i++, i3 += 3) {
            indices[i] = glm::uvec3(integerIndices[i3 + 0], integerIndices[i3 + 1], integerIndices[i3 + 2]);
        }
        integerIndices.clear();

        std::vector<glm::vec3> positions = mutParams.getMove<std::vector<glm::vec3>>("P");
        std::vector<glm::vec3> normals;
        if (params.contains("N"))
            normals = mutParams.getMove<std::vector<glm::vec3>>("N");
        std::vector<glm::vec2> texCoords;
        if (params.contains("st")) {
            texCoords = mutParams.getMove<std::vector<glm::vec2>>("st");
        }

        std::shared_ptr<pandora::TriangleShape> pShape;
        if (transform == glm::identity<glm::mat4>()) {
            pShape = std::make_shared<pandora::TriangleShape>(
                std::move(indices),
                std::move(positions),
                std::move(normals),
                std::move(texCoords));
        } else {
            pShape = std::make_shared<pandora::TriangleShape>(
                std::move(indices),
                std::move(positions),
                std::move(normals),
                std::move(texCoords),
                transform);
        }

        // Cache builders & serializers are not thread-safe so serializing/storing the geometry should happen sequentially.
        // TODO: make cache builders & serializers thread-safe because this lock may cause a serious bottleneck.
        if (scene.pGeometryCacheBuilder) {
            std::unique_lock l { scene.geometryCacheMutex };
            scene.pGeometryCacheBuilder->registerCacheable(pShape.get(), true);
        }

        pSceneObject->pShape = pShape;
    });
}

void Parser::parsePlymesh(PBRTIntermediateScene& scene, Params&& params)
{
    // Create scene object with deferred shape
    std::shared_ptr<pandora::SceneObject> pSceneObject;
    if (m_graphicsState.emittedAreaLight) {
        auto pAreaLight = std::make_unique<pandora::AreaLight>(*m_graphicsState.emittedAreaLight);
        pSceneObject = scene.sceneBuilder.addSceneObject(nullptr, m_graphicsState.pMaterial, std::move(pAreaLight));
    } else {
        pSceneObject = scene.sceneBuilder.addSceneObject(nullptr, m_graphicsState.pMaterial);
    }

    // Add to scene if not an instance template
    if (!m_currentObject) {
        scene.sceneBuilder.attachObjectToRoot(pSceneObject);
    } else {
        scene.sceneBuilder.attachObject(m_currentObject->pSceneNode, pSceneObject);
    }

    // Make a copy of the lexer source so it doesn't immediately get deleted when
    // an include statement is encountered. This is a trade-off so we can keep working
    // with std::string_view's instead of making copies.
    m_asyncWorkTaskGroup.run([basePath = m_basePath, transform = m_currentTransform, pSceneObject, params = std::move(params), pLexerSource = m_pCurrentLexerSource, &scene]() {
        // Load mesh
        auto filePath = basePath / params.get<std::string_view>("filename");
        std::optional<pandora::TriangleShape> shapeOpt;
        if (transform == glm::identity<glm::mat4>())
            shapeOpt = pandora::TriangleShape::loadFromFileSingleShape(filePath);
        else
            shapeOpt = pandora::TriangleShape::loadFromFileSingleShape(filePath, transform);
        if (!shapeOpt) {
            spdlog::error("Failed to load plymesh \"{}\"", filePath.string());
            return;
        }
        auto pShape = std::make_shared<pandora::TriangleShape>(std::move(*shapeOpt));

        // Cache builders & serializers are not thread-safe so serializing/storing the geometry should happen sequentially.
        // TODO: make cache builders & serializers thread-safe because this lock may cause a serious bottleneck.
        if (scene.pGeometryCacheBuilder) {
            std::unique_lock l { scene.geometryCacheMutex };
            scene.pGeometryCacheBuilder->registerCacheable(pShape.get(), true);
        }

        pSceneObject->pShape = pShape;
    });
}

void Parser::parseTexture()
{
    std::string_view textureName = next().text;
    std::string_view textureType = next().text;
    std::string_view mapType = next().text;
    auto params = parseParams();

    if (textureType == "float") {
        std::shared_ptr<pandora::Texture<float>> pTexture;
        if (mapType == "constant") {
            float value = params.get<float>("value", 1.0f);
            pTexture = m_textureCache.getConstantTexture(value);
        } else if (mapType == "imagemap" && m_loadTextures) {
            std::string_view fileName = params.get<std::string_view>("filename");
            std::filesystem::path filePath = m_basePath / fileName;
            if (m_loadTextures)
                pTexture = m_textureCache.getImageTexture<float>(filePath);
            else
                pTexture = m_textureCache.getConstantTexture(1.0f);
        } else {
            //spdlog::warn("Replacing texture with unsupported map type \"{}\" by constant texture", mapType);
            pTexture = m_textureCache.getConstantTexture(1.0f);
        }
        m_namedFloatTextures[std::string(textureName)] = pTexture;
    } else if (textureType == "spectrum" || textureType == "rgb" || textureType == "color") {
        std::shared_ptr<pandora::Texture<glm::vec3>> pTexture;
        if (mapType == "constant") {
            glm::vec3 value = params.get<glm::vec3>("value", glm::vec3(1.0f));
            pTexture = m_textureCache.getConstantTexture(value);
        } else if (mapType == "imagemap" && m_loadTextures) {
            std::string_view fileName = params.get<std::string_view>("filename");
            std::filesystem::path filePath = m_basePath / fileName;
            if (m_loadTextures)
                pTexture = m_textureCache.getImageTexture<glm::vec3>(filePath);
            else
                pTexture = m_textureCache.getConstantTexture(glm::vec3(1.0f));
        } else {
            //spdlog::warn("Replacing texture with unsupported map type \"{}\" by constant texture", mapType);
            pTexture = m_textureCache.getConstantTexture(glm::vec3(1.0f));
        }
        m_namedVec3Textures[std::string(textureName)] = pTexture;
    } else {
        spdlog::error("Unsupported texture type \"{}\"", textureType);
    }
}

std::shared_ptr<pandora::Material> Parser::parseMaterial(const Token& typeToken) noexcept
{
    auto params = parseParams();
    const std::string_view materialType = typeToken ? typeToken.text : params.get<std::string_view>("type");

    if (materialType == "matte") {
        std::shared_ptr<pandora::Texture<glm::vec3>> pKdTexture;
        if (!params.contains("Kd") || params.isOfType<glm::vec3>("Kd")) {
            const glm::vec3 kd = params.get<glm::vec3>("Kd", glm::vec3(0.5f));
            pKdTexture = m_textureCache.getConstantTexture(kd);
        } else {
            const std::string_view textureName = params.get<std::string_view>("Kd");
            pKdTexture = m_namedVec3Textures.find(std::string(textureName))->second;
        }

        std::shared_ptr<pandora::Texture<float>> pSigmaTexture;
        if (!params.contains("sigma") || params.isOfType<float>("sigma")) {
            const float sigma = params.get<float>("sigma", 0.0f);
            pSigmaTexture = m_textureCache.getConstantTexture(sigma);
        } else {
            const std::string_view textureName = params.get<std::string_view>("sigma");
            pSigmaTexture = m_namedFloatTextures.find(std::string(textureName))->second;
        }
        return std::make_shared<pandora::MatteMaterial>(pKdTexture, pSigmaTexture);
    } else {
        spdlog::warn("Replacing material of unsupported type \"{}\" by matte white", materialType);
        const auto pKdTexture = m_textureCache.getConstantTexture(glm::vec3(0.5f));
        const auto pSigmaTexture = m_textureCache.getConstantTexture(0.0f);
        return std::make_shared<pandora::MatteMaterial>(pKdTexture, pSigmaTexture);
    }
}

bool Parser::parseTransform(const Token& token) noexcept
{
    if (token.type != TokenType::LITERAL)
        return false;

    if (token == "ActiveTransform") {
        const std::string_view which = next().text;
        (void)which;

        spdlog::warn("\"ActiveTransform {}\" encountered. Motion blur is not supported in Pandora yet", which);

        if (which == "All") {
            m_transformStartActive = true;
        } else if (which == "StartTime") {
            m_transformStartActive = true;
        } else if (which == "EndTime") {
            m_transformStartActive = false;
        } else {
            spdlog::error("Unknown argument \"{}\" to \"ActiveTransform\" command", which);
        }

        pushTransform();
        return true;
    }
    if (token == "TransformBegin") {
        pushTransform();
        return true;
    }
    if (token == "TransformEnd") {
        popTransform();
        return true;
    }
    if (token == "LookAt") {
        const glm::vec3 eye = parse<glm::vec3>();
        const glm::vec3 target = parse<glm::vec3>();
        const glm::vec3 up = parse<glm::vec3>();
        glm::mat4 transform = glm::lookAtLH(eye, target, up);
        m_currentTransform = glm::scale(glm::mat4(1), glm::vec3(1, -1, 1)) * transform;
        return true;
    }
    if (token == "Scale") {
        const glm::vec3 scale = parse<glm::vec3>();
        m_currentTransform = glm::scale(m_currentTransform, scale);
        return true;
    }
    if (token == "Translate") {
        const glm::vec3 translation = parse<glm::vec3>();
        m_currentTransform = glm::translate(m_currentTransform, translation);
        return true;
    }
    if (token == "ConcatTransform") {
        m_currentTransform *= parse<glm::mat4>();
        return true;
    }
    if (token == "Rotate") {
        const float angle = parse<float>();
        const glm::vec3 axis = parse<glm::vec3>();
        m_currentTransform = glm::rotate(m_currentTransform, angle, axis);
        return true;
    }
    if (token == "Transform") {
        m_currentTransform = parse<glm::mat4>();
        return true;
    }
    if (token == "Identity") {
        m_currentTransform = glm::identity<glm::mat4>();
        return true;
    }
    if (token == "ReverseOrientation") {
        spdlog::warn("ReverseOrientation encountered but not implemented yet");
        return true;
    }
    if (token == "CoordSysTransform") {
        spdlog::warn("CoordSysTransform encountered but not implemented yet");
        (void)next();
        return true;
    }

    return false;
}

void Parser::pushAttributes() noexcept
{
    m_graphicsStateStack.push(m_graphicsState);
    pushTransform();
}

void Parser::popAttributes() noexcept
{
    m_graphicsState = std::move(m_graphicsStateStack.top());
    m_graphicsStateStack.pop();
    popTransform();
}

void Parser::pushTransform() noexcept
{
    m_transformStack.push(m_currentTransform);
}

void Parser::popTransform() noexcept
{
    m_currentTransform = m_transformStack.top();
    m_transformStack.pop();
}

Params Parser::parseParams()
{
    Params params;
    while (true) {
        Token token = peek();

        if (token.type != TokenType::STRING)
            return params;

        const auto&& [paramType, paramNameTmp] = splitStringFirstWhitespace(next().text);
        const std::string_view paramName = paramNameTmp; // Lambdas are not allowed to reference local binding

        auto addParam = [&](auto v) {
            params.add(paramName, v);
        };
        auto addParamPossibleArray = [&](auto v) {
            // v is std::variant<T, std::vector<T>>;
            if (v.index() == 0)
                params.add(paramName, std::get<0>(v));
            else
                params.add(paramName, std::get<1>(std::move(v)));
        };

        if (paramType == "float") {
            addParamPossibleArray(parseParamPossibleArray<float>());
        } else if (paramType == "color") {
            addParam(parseParam<glm::vec3>());
        } else if (paramType == "blackbody") {
            spdlog::warn("Blackbody type not suported, replacing by white");
            (void)next(); // List begin token
            (void)next(); // Color temperature in Kelvin
            const float scale = parse<float>();
            (void)next(); // List end token
            addParam(glm::vec3(scale));
        } else if (paramType == "rgb") {
            addParam(parseParam<glm::vec3>());
        } else if (paramType == "spectrum") {
            addParam(parseParam<glm::vec3>());
        } else if (paramType == "integer") {
            addParamPossibleArray(parseParamPossibleArray<int>());
        } else if (paramType == "bool") {
            addParam(parseParam<std::string_view>() == "true");
        } else if (paramType == "texture") {
            addParam(parseParam<std::string_view>());
        } else if (paramType == "normal") {
            addParamPossibleArray(parseParamPossibleArray<glm::vec3>());
        } else if (paramType == "point") {
            addParamPossibleArray(parseParamPossibleArray<glm::vec3>());
        } else if (paramType == "point2") {
            addParamPossibleArray(parseParamPossibleArray<glm::vec2>());
        } else if (paramType == "point3") {
            addParamPossibleArray(parseParamPossibleArray<glm::vec3>());
        } else if (paramType == "vector") {
            addParamPossibleArray(parseParamPossibleArray<glm::vec3>());
        } else if (paramType == "string") {
            addParam(parseParam<std::string_view>());
        } else {
            spdlog::error("Ignoring param \"{}\" with unknown type \"{}\"", paramName, paramType);
            if (peek().type == TokenType::LIST_BEGIN) {
                while (next().type != TokenType::LIST_END)
                    ;
            } else {
                while (peek().type != TokenType::LITERAL)
                    (void)next();
            }
        }
    }
}

Token Parser::next()
{
    Token token = peek();
    if (!token) {
        spdlog::error("Unexpected end of file");
        std::terminate();
    }

    m_peekQueue.pop_front();
    return token;
}

static std::pair<size_t, std::string_view> formatByteSize(size_t sizeBytes)
{
    if (sizeBytes < 1024) {
        return { sizeBytes, "B" };
    } else if (sizeBytes < 1024 * 1024) {
        return { sizeBytes / 1024, "KB" };
    } else if (sizeBytes < 1024 * 1024 * 1024) {
        return { sizeBytes / (1024 * 1024), "MB" };
    } else {
        return { sizeBytes / (1024 * 1024 * 1024), "GB" };
    }
}

Token Parser::peek(unsigned i)
{
    while (m_peekQueue.size() <= i) {
        Token token = m_currentLexer.next();

        if (token && token.text == "Include") {
            Token fileNameToken = m_currentLexer.next();
            auto includedFilePath = m_basePath / fileNameToken.text;

            const auto [fileSize, fileSizeType] = formatByteSize(std::filesystem::file_size(includedFilePath));
            spdlog::info("Including file \"{}\" ({} {})", includedFilePath.string(), fileSize, fileSizeType);

            addLexer(includedFilePath);
            continue;
        }

        if (token) {
            m_peekQueue.push_back(token);
            continue;
        }

        // Encountered end of file
        if (m_lexerStack.empty()) {
            // Nothing to back off to, return end of file indicator
            return Token();
        }

        std::tie(m_pCurrentLexerSource, m_currentLexer) = std::move(m_lexerStack.top());
        m_lexerStack.pop();
        continue;
    }

    return m_peekQueue[i];
}

void Parser::addLexer(std::filesystem::path file)
{
    if (m_pCurrentLexerSource)
        m_lexerStack.push({ std::move(m_pCurrentLexerSource), std::move(m_currentLexer) });

    m_pCurrentLexerSource = std::make_shared<mio::mmap_source>(file.string());
    const std::string_view fileContents { m_pCurrentLexerSource->data(), m_pCurrentLexerSource->length() };
    m_currentLexer = SIMDLexer(fileContents);
}

inline bool isWhiteSpace(const char c) noexcept
{
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

std::pair<std::string_view, std::string_view> splitStringFirstWhitespace(std::string_view string)
{
    int i = 0;
    while (!isWhiteSpace(string[++i]))
        ;

    std::string_view subString1 = string.substr(0, i);

    while (isWhiteSpace(string[++i]))
        ;

    std::string_view subString2 = string.substr(i);
    return { subString1, subString2 };
}
