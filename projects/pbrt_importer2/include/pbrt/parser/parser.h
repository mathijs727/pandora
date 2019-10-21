#pragma once
#include "pbrt/lexer/lexer.h"
#include <EASTL/fixed_hash_map.h>
#include <charconv>
#include <deque>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <mio/mmap.hpp>
#include <pandora/graphics_core/pandora.h>
#include <pandora/graphics_core/perspective_camera.h>
#include <pandora/graphics_core/scene.h>
#include <pandora/graphics_core/sensor.h>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Parser.h

struct PBRTScene {
    pandora::Scene scene;
    std::vector<std::unique_ptr<pandora::PerspectiveCamera>> cameras;
    std::unique_ptr<pandora::Sensor> pSensor;
};

class Parser {
public:
    Parser(std::filesystem::path basePath);

    PBRTScene parse(std::filesystem::path file);

private:
    using ParamValue = std::variant<
        int,
        float,
        glm::vec3,
        glm::mat4,
        std::string_view,
        std::vector<int>,
        std::vector<float>,
        std::vector<glm::vec3>>;
    class Params {
    public:
        template <typename T>
        inline void add(std::string_view key, T&& value)
        {
            m_values[key] = std::forward<T>(value);
        }

        template <typename T>
        inline T get(std::string_view key) const
        {
            return std::get<T>(m_values.find(key)->second);
        }
        template <typename T>
        inline T get(std::string_view key, const T& default) const
        {
            if (auto iter = m_values.find(key); iter != std::end(m_values)) {
                return std::get<T>(iter->second);
            } else {
                return default;
            }
        }

    private:
        std::unordered_map<std::string_view, ParamValue> m_values;
    };

    // Parse everything in WorldBegin/WorldEnd
    void parseWorld();

    // Parse everything in the root scene file
    void parseScene();

    bool parseTransform(const Token& token) noexcept;

    void pushAttributes() noexcept;
    void popAttributes() noexcept;

    void pushTransform() noexcept;
    void popTransform() noexcept;

    template <typename T>
    T parse() noexcept;

    template <typename T>
    T parseParam();
    template <typename T>
    std::vector<T> parseParamArray();
    template <typename T>
    std::variant<T, std::vector<T>> parseParamPossibleArray();
    Params parseParams();

    Token next();
    Token peek(unsigned ahead = 0);
    void addLexer(std::filesystem::path file);

private:
    std::filesystem::path m_basePath;
    std::stack<std::pair<mio::mmap_source, Lexer>> m_lexerStack;
	mio::mmap_source m_currentLexerSource;
    Lexer m_currentLexer;
    std::deque<Token> m_peekQueue;

    bool m_transformStartActive { true };
    glm::mat4 m_currentTransform { glm::identity<glm::mat4>() };
    std::stack<glm::mat4> m_transformStack;

    // We cannot create camera's as we parse because we need to compute the aspect ratio
    // based on the film/sensor which might not be read a camera is defined.
    std::vector<std::pair<glm::mat4, Params>> m_cameraParams;
    std::unique_ptr<pandora::Sensor> m_pSensor;
    pandora::SceneBuilder m_pandoraSceneBuilder;

    std::stack<pandora::Material*> m_materialStack;
    std::stack<pandora::SceneNode*> m_objectStack;

    //pandora::SceneBuilder m_pandoraSceneBuilder;
    //std::unordered_map<std::string, pandora::SceneNode> m_namedObjects;
};

template <>
inline std::string_view Parser::parse<std::string_view>() noexcept
{
    return next().text;
}

template <>
inline int Parser::parse<int>() noexcept
{
    Token token = next();

    int result;
    std::from_chars(token.text.data(), token.text.data() + token.text.length(), result);
    return result;
}

template <>
inline float Parser::parse<float>() noexcept
{
    Token token = next();

    float result;
    std::from_chars(token.text.data(), token.text.data() + token.text.length(), result);
    return result;
}

template <>
inline glm::vec3 Parser::parse<glm::vec3>() noexcept
{
    const float x = parse<float>();
    const float y = parse<float>();
    const float z = parse<float>();
    return glm::vec3(x, y, z);
}

template <>
inline glm::mat4 Parser::parse<glm::mat4>() noexcept
{
    const std::string_view bracketOpen = next().text;
    assert(bracketOpen == "[");

    glm::mat4 out;
    out[0][0] = parse<float>();
    out[0][1] = parse<float>();
    out[0][2] = parse<float>();
    out[0][3] = parse<float>();

    out[1][0] = parse<float>();
    out[1][1] = parse<float>();
    out[1][2] = parse<float>();
    out[1][3] = parse<float>();

    out[2][0] = parse<float>();
    out[2][1] = parse<float>();
    out[2][2] = parse<float>();
    out[2][3] = parse<float>();

    out[3][0] = parse<float>();
    out[3][1] = parse<float>();
    out[3][2] = parse<float>();
    out[3][3] = parse<float>();

    const auto nextTokenType = next().type;
    assert(nextTokenType == TokenType::LIST_END);

    return out;
}

template <typename T>
inline T Parser::parseParam()
{
    if (peek().type == TokenType::LIST_BEGIN) {
        (void)next(); // Read LIST_BEGIN token
        auto result = parse<T>();

        assert(peek().type == TokenType::LIST_END);
        while (next().type != TokenType::LIST_END)
            ;

        return result;
    } else {
        return parse<T>();
    }
}

template <typename T>
inline std::vector<T> Parser::parseParamArray()
{
    auto listBeginToken = next();
    assert(listBeginToken.type == TokenType::LIST_BEGIN);

    std::vector<T> res;
    while (peek().type != TokenType::LIST_END) {
        res.push_back(parse<T>());
    }

    auto listEndToken = next();
    assert(listEndToken.type == TokenType::LIST_END);
    return res;
}

template <typename T>
inline std::variant<T, std::vector<T>> Parser::parseParamPossibleArray()
{
    if (peek().type == TokenType::LIST_BEGIN) {
        return parseParamArray<T>();
    } else {
        return parse<T>();
    }
}
