#include "pbrt/parser/parser.h"
#include <charconv>
#include <glm/gtc/matrix_transform.hpp>
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

    m_cameraParams.clear();
    m_pSensor = nullptr;
    m_pandoraSceneBuilder = pandora::SceneBuilder();

    parseScene();

    assert(m_pSensor);
    const glm::vec2 fResolution = m_pSensor->getResolution();
    const float aspectRatio = fResolution.x / fResolution.y;
    std::vector<std::unique_ptr<pandora::PerspectiveCamera>> cameras;
    std::transform(std::begin(m_cameraParams), std::end(m_cameraParams), std::back_inserter(cameras),
        [=](auto cameraData) {
            const auto& [worldToCamera, cameraParams] = cameraData;

            // PBRT defines field of view along shortest axis
            float fov = 2.0f * cameraParams.get<float>("fov", 45.0f);
            if (fResolution.x > fResolution.y)
                fov = std::atan(std::tan(fov / 2.0f) * aspectRatio) * 2.0f;

            return std::make_unique<pandora::PerspectiveCamera>(aspectRatio, fov, glm::inverse(worldToCamera));
        });

    return PBRTScene { m_pandoraSceneBuilder.build(), std::move(cameras), std::move(m_pSensor) };
}

void Parser::parseWorld()
{
    while (true) {
        Token token = next();
        assert(token);

        if (token == "WorldEnd")
            break;
    }
}

void Parser::parseScene()
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

            m_cameraParams.push_back({ m_currentTransform, std::move(params) });
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
            assert(!m_pSensor);

            const int resolutionX = params.get<int>("xresolution", 640);
            const int resolutionY = params.get<int>("yresolution", 480);
            m_pSensor = std::make_unique<pandora::Sensor>(glm::ivec2(resolutionX, resolutionY));
            continue;
        }
        if (token == "WorldBegin") {
            // Reset CTM
            m_currentTransform = glm::identity<glm::mat4>();
            while (!m_transformStack.empty())
                m_transformStack.pop();
            m_transformStartActive = true;

            parseWorld();
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
}

void Parser::popAttributes() noexcept
{
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

Parser::Params Parser::parseParams()
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
            (void)next(); // Color temperature in Kelvin
            const float scale = parse<float>();
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
