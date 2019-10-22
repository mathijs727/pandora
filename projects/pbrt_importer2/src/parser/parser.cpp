#include "pbrt/parser/parser.h"
#include <charconv>
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

Parser::Parser(std::filesystem::path basePath)
    : m_basePath(basePath)
{
}

PBRTScene Parser::parse(std::filesystem::path file)
{
    addLexer(file);

    PBRTIntermediateScene intermediateScene;
    parseScene(intermediateScene);

    const glm::vec2 fResolution = intermediateScene.resolution;
    const float aspectRatio = fResolution.x / fResolution.y;
    std::vector<pandora::PerspectiveCamera> cameras;
    std::transform(std::begin(intermediateScene.cameras), std::end(intermediateScene.cameras), std::back_inserter(cameras),
        [=](auto cameraData) {
            const auto& [worldToCamera, cameraParams] = cameraData;

            // PBRT defines field of view along shortest axis
            float fov = 2.0f * cameraParams.get<float>("fov", 45.0f);
            if (fResolution.x > fResolution.y)
                fov = std::atan(std::tan(fov / 2.0f) * aspectRatio) * 2.0f;

            return pandora::PerspectiveCamera(aspectRatio, fov, glm::inverse(worldToCamera));
        });

    return PBRTScene { intermediateScene.sceneBuilder.build(), std::move(cameras), intermediateScene.resolution };
}

void Parser::parseWorld(PBRTIntermediateScene& scene)
{
    while (true) {
        Token token = next();
        assert(token);

        if (token == "WorldEnd")
            break;

        if (token == "AttributeBegin") {
            pushAttributes();
            continue;
        }
        if (token == "AttributeEnd") {
            popAttributes();
            continue;
        }

        if (token == "LightSource") {
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
                if (params.contains("mapname")) {
                    pTexture = scene.textureCache.getImageTexture<glm::vec3>(m_basePath / params.get<std::string_view>("mapname"));
                } else {
                    pTexture = scene.textureCache.getConstantTexture(glm::vec3(1));
                }
            } else {
                spdlog::warn("Ignoring light of unsupported type \"{}\"", lightType);
            }
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
            std::string_view materialType = next().text;
            auto params = parseParams();

            if (materialType == "matte") {
                const glm::vec3 kd = params.get<glm::vec3>("Kd", glm::vec3(0.5f));
                const float sigma = params.get<float>("sigma", 0.0f);
                const auto pKdTexture = scene.textureCache.getConstantTexture(kd);
                const auto pSigmaTexture = scene.textureCache.getConstantTexture(sigma);
                m_graphicsState.pMaterial = std::make_shared<pandora::MatteMaterial>(pKdTexture, pSigmaTexture);
            } else {
                spdlog::warn("Replacing material of unsupported type \"{}\" by matte white", materialType);
                const auto pKdTexture = scene.textureCache.getConstantTexture(0.5f);
                const auto pSigmaTexture = scene.textureCache.getConstantTexture(0.0f);
                m_graphicsState.pMaterial = std::make_shared<pandora::MatteMaterial>(pKdTexture, pSigmaTexture);
            }
        }
        if (token == "Shape") {
            std::string_view shapeType = next().text;
            auto params = parseParams();

            if (shapeType == "plymesh") {
                // Load mesh
                auto filePath = m_basePath / params.get<std::string_view>("filename");
                auto shapeOpt = pandora::TriangleShape::loadFromFileSingleShape(filePath, m_currentTransform);
                if (!shapeOpt) {
                    spdlog::error("Failed to load plymesh \"{}\"", filePath.string());
                    continue;
                }
                auto pShape = std::make_shared<pandora::TriangleShape>(std::move(*shapeOpt));

                // Create scene object
                std::shared_ptr<pandora::SceneObject> pSceneObject;
                if (m_graphicsState.emittedAreaLight) {
                    pSceneObject = scene.sceneBuilder.addSceneObject(pShape, m_graphicsState.pMaterial);
                } else {
                    auto pAreaLight = std::make_unique<pandora::AreaLight>(*m_graphicsState.emittedAreaLight);
                    pSceneObject = scene.sceneBuilder.addSceneObject(pShape, m_graphicsState.pMaterial, std::move(pAreaLight));
                }

                // Add to scene if not an instance template
                if (!m_currentObject) {
                    scene.sceneBuilder.attachObjectToRoot(pSceneObject);
                } else {
                    scene.sceneBuilder.attachObject(m_currentObject->pSceneNode, pSceneObject);
                }
            } else if (shapeType == "trianglemesh") {
                // Load mesh
                std::vector<int> integerIndices = std::move(params.get<std::vector<int>>("indices"));
                assert(integerIndices.size() % 3 == 0);
                std::vector<glm::uvec3> indices;
                indices.resize(integerIndices.size() / 3);
                for (size_t i = 0, i3 = 0; i < indices.size(); i++, i3 += 3) {
                    indices[i] = glm::uvec3(integerIndices[i3 + 0], integerIndices[i3 + 1], integerIndices[i3 + 2]);
                }
                integerIndices.clear();

                std::vector<glm::vec3> positions = std::move(params.get<std::vector<glm::vec3>>("P"));
                std::vector<glm::vec3> normals;
                if (params.contains("N"))
                    normals = std::move(params.get<std::vector<glm::vec3>>("N"));
                std::vector<glm::vec2> uvCoords;
                if (params.contains("uv")) {
                    auto floatUvCoords = std::move(params.get<std::vector<float>>("uv"));

                    uvCoords.resize(indices.size());
                    for (size_t i = 0, i2 = 0; i < uvCoords.size(); i++, i2 += 2) {
                        uvCoords[i] = glm::vec2(floatUvCoords[i2 + 0], floatUvCoords[i2 + 1]);
                    }
                    floatUvCoords.clear();
                }
                std::vector<glm::vec3> tangents;
                if (params.contains("S"))
                    tangents = std::move(params.get<std::vector<glm::vec3>>("S"));

                auto pShape = std::make_shared<pandora::TriangleShape>(
                    std::move(indices),
                    std::move(positions),
                    std::move(normals),
                    std::move(uvCoords),
                    std::move(tangents));

                // Create scene object
                std::shared_ptr<pandora::SceneObject> pSceneObject;
                if (m_graphicsState.emittedAreaLight) {
                    pSceneObject = scene.sceneBuilder.addSceneObject(pShape, m_graphicsState.pMaterial);
                } else {
                    auto pAreaLight = std::make_unique<pandora::AreaLight>(*m_graphicsState.emittedAreaLight);
                    pSceneObject = scene.sceneBuilder.addSceneObject(pShape, m_graphicsState.pMaterial, std::move(pAreaLight));
                }

                // Add to scene if not an instance template
                if (!m_currentObject) {
                    scene.sceneBuilder.attachObjectToRoot(pSceneObject);
                } else {
                    scene.sceneBuilder.attachObject(m_currentObject->pSceneNode, pSceneObject);
                }
            } else {
                spdlog::warn("Ignoring shape of unsupported type \"{}\"", shapeType);
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
        glm::vec3 eye = parse<glm::vec3>();
        glm::vec3 target = parse<glm::vec3>();
        glm::vec3 up = parse<glm::vec3>();
        m_currentTransform *= glm::lookAt(eye, target, up);
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

        auto&& [paramType, paramName] = splitStringFirstWhitespace(next().text);
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

Token Parser::peek(unsigned i)
{
    while (m_peekQueue.size() <= i) {
        Token token = m_currentLexer.next();

        if (token && token.text == "Include") {
            Token fileNameToken = m_currentLexer.next();
            auto includedFilePath = m_basePath / fileNameToken.text;
            spdlog::info("Including file \"{}\"", includedFilePath.string());

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

        m_currentLexer = std::move(std::get<1>(m_lexerStack.top()));
        m_lexerStack.pop();
        continue;
    }

    return m_peekQueue[i];
}

void Parser::addLexer(std::filesystem::path file)
{
    m_lexerStack.push({ std::move(m_currentLexerSource), std::move(m_currentLexer) });

    m_currentLexerSource = mio::mmap_source(file.string());
    const std::string_view fileContents { m_currentLexerSource.data(), m_currentLexerSource.length() };
    m_currentLexer = Lexer(fileContents);
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
