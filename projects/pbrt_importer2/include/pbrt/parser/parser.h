#pragma once
#include "crack_atof.h"
#include "params.h"
#include "pbrt/lexer/lexer.h"
#include "pbrt/lexer/simd_lexer.h"
#include "texture_cache.h"
#include <EASTL/fixed_hash_map.h>
#include <EASTL/fixed_vector.h>
#include <algorithm>
#include <charconv>
#include <deque>
#include <execution>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <mio/mmap.hpp>
#include <pandora/graphics_core/load_from_file.h>
#include <pandora/graphics_core/pandora.h>
#include <pandora/graphics_core/perspective_camera.h>
#include <pandora/graphics_core/scene.h>
#include <pandora/graphics_core/sensor.h>
#include <pandora/textures/image_texture.h>
#include <stack>
#include <stream/cache/lru_cache.h>
#include <tbb/task_group.h>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>
#include <mutex>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Parser.h

// Camera creation has to be deferred because it relies on the aspect ratio of the output (defined by Film command)
struct PBRTCamera {
    glm::mat4 transform;
    Params createParams;
};

struct PBRTIntermediateScene {
    pandora::SceneBuilder sceneBuilder;

	std::mutex geometryCacheMutex;
    stream::CacheBuilder* pGeometryCacheBuilder;

    std::vector<PBRTCamera> cameras;
    glm::ivec2 resolution;
};

struct GraphicsState {
    // AreaLight has no copy constructor???
    std::unordered_map<std::string, std::shared_ptr<pandora::Material>> namedMaterials;
    std::unordered_map<std::string, std::shared_ptr<pandora::Texture<float>>> namedFloatTextures;
    std::unordered_map<std::string, std::shared_ptr<pandora::Texture<glm::vec3>>> namedVec3Textures;

    std::optional<glm::vec3> emittedAreaLight;
    std::shared_ptr<pandora::Material> pMaterial;

    bool reverseOrientation { false };
};

class Parser {
public:
    Parser(std::filesystem::path basePath, bool loadTextures = true);

    pandora::RenderConfig parse(std::filesystem::path file, stream::CacheBuilder* pGeometryCacheBuilder = nullptr);

private:
    // Parse everything in WorldBegin/WorldEnd
    void parseWorld(PBRTIntermediateScene& scene);
    // Parse everything in the root scene file
    void parseScene(PBRTIntermediateScene& scene);

    std::shared_ptr<pandora::Material> parseMaterial(const Token& tokenType) noexcept;
    void parseLightSource(PBRTIntermediateScene& scene);
    void parseShape(PBRTIntermediateScene& scene);
    void parseTriangleShape(PBRTIntermediateScene& scene, Params&& params);
    void parsePlymesh(PBRTIntermediateScene& scene, Params&& params);
    void parseTexture();
    bool parseTransform(const Token& token) noexcept;

    void pushAttributes() noexcept;
    void popAttributes() noexcept;

    void pushTransform() noexcept;
    void popTransform() noexcept;

    template <typename T>
    static T parse(std::string_view string) noexcept;
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
    const bool m_loadTextures;
    const std::filesystem::path m_basePath;

    std::stack<std::pair<std::shared_ptr<mio::mmap_source>, SIMDLexer>> m_lexerStack;
    std::shared_ptr<mio::mmap_source> m_pCurrentLexerSource;
    SIMDLexer m_currentLexer;
    std::deque<Token> m_peekQueue;

    tbb::task_group m_asyncWorkTaskGroup;

    // CTM
    bool m_transformStartActive { true };
    glm::mat4 m_currentTransform { glm::identity<glm::mat4>() };
    std::stack<glm::mat4> m_transformStack;

    // Graphics state stack
    GraphicsState m_graphicsState;
    std::stack<GraphicsState> m_graphicsStateStack;

    // Object stack
    struct Object {
        //std::shared_ptr<pandora::Material> pMaterial;
        std::string name;
        std::shared_ptr<pandora::SceneNode> pSceneNode;
        //glm::mat4 transform;
    };
    std::stack<Object> m_objectStack;
    std::optional<Object> m_currentObject;
    std::unordered_map<std::string, std::shared_ptr<pandora::SceneNode>> m_objects;

    TextureCache m_textureCache;
    std::unordered_map<std::string, std::shared_ptr<pandora::Texture<float>>> m_namedFloatTextures;
    std::unordered_map<std::string, std::shared_ptr<pandora::Texture<glm::vec3>>> m_namedVec3Textures;
    //pandora::SceneBuilder m_pandoraSceneBuilder;
    //std::unordered_map<std::string, pandora::SceneNode> m_namedObjects;
};

template <>
inline std::string_view Parser::parse<std::string_view>() noexcept
{
    return next().text;
}

template <>
inline int Parser::parse<int>(std::string_view tokenText) noexcept
{
    int result;
    std::from_chars(tokenText.data(), tokenText.data() + tokenText.length(), result);
    return result;
}

template <>
inline float Parser::parse<float>(std::string_view tokenText) noexcept
{
    // Float parsing is a significant bottleneck in the WDAS Moana scene. Use a string-to-float parser that is faster
    // than MSVCs std::from_chars (tested at MSVC version 19.23.28106.4).
    // Float parser copied from: https://github.com/shaovoon/floatbench
    //float result;
    //std::from_chars(tokenText.data(), tokenText.data() + tokenText.length(), result);
    //return result;
    return static_cast<float>(crackAtof(tokenText));
}

template <>
inline int Parser::parse<int>() noexcept
{
    return parse<int>(next().text);
}

template <>
inline float Parser::parse<float>() noexcept
{
    return parse<float>(next().text);
}

template <>
inline glm::vec2 Parser::parse<glm::vec2>() noexcept
{
    const float x = parse<float>();
    const float y = parse<float>();
    return glm::vec2(x, y);
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
    static constexpr size_t parallelThreshold = 1000;

    auto listBeginToken = next();
    assert(listBeginToken.type == TokenType::LIST_BEGIN);

    std::vector<T> res;
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>) {
        eastl::fixed_vector<std::string_view, 8> tokenStrings;
        while (peek().type != TokenType::LIST_END) {
            tokenStrings.emplace_back(next().text);
        }

        res.resize(tokenStrings.size());
        constexpr auto operation = [](const std::string_view string) {
            return parse<T>(string);
        };
        if (tokenStrings.size() > parallelThreshold) {
            std::transform(std::execution::par_unseq, std::begin(tokenStrings), std::end(tokenStrings), std::begin(res), operation);
        } else {
            std::transform(std::execution::seq, std::begin(tokenStrings), std::end(tokenStrings), std::begin(res), operation);
        }
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
        eastl::fixed_vector<std::tuple<std::string_view, std::string_view>, 8> tokenStrings;
        while (peek().type != TokenType::LIST_END) {
            auto x = next().text;
            auto y = next().text;
            tokenStrings.emplace_back(x, y);
        }

        res.resize(tokenStrings.size());
        constexpr auto operation = [](const auto& strings) {
            return glm::vec2(parse<float>(std::get<0>(strings)), parse<float>(std::get<1>(strings)));
        };
        if (tokenStrings.size() > parallelThreshold / 2) {
            std::transform(std::execution::par_unseq, std::begin(tokenStrings), std::end(tokenStrings), std::begin(res), operation);
        } else {
            std::transform(std::execution::seq, std::begin(tokenStrings), std::end(tokenStrings), std::begin(res), operation);
        }
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        eastl::fixed_vector<std::tuple<std::string_view, std::string_view, std::string_view>, 8> tokenStrings;
        while (peek().type != TokenType::LIST_END) {
            auto x = next().text;
            auto y = next().text;
            auto z = next().text;
            tokenStrings.emplace_back(x, y, z);
        }

        res.resize(tokenStrings.size());
        constexpr auto operation = [](const auto& strings) {
            return glm::vec3(parse<float>(std::get<0>(strings)), parse<float>(std::get<1>(strings)), parse<float>(std::get<2>(strings)));
        };
        if (tokenStrings.size() > parallelThreshold / 3) {
            std::transform(std::execution::par_unseq, std::begin(tokenStrings), std::end(tokenStrings), std::begin(res), operation);
        } else {
            std::transform(std::execution::seq, std::begin(tokenStrings), std::end(tokenStrings), std::begin(res), operation);
        }
    } else {
        while (peek().type != TokenType::LIST_END) {
            res.push_back(parse<T>());
        }
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
