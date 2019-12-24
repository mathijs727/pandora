#pragma once
#include "params.h"
#include "pbrt/lexer/lexer.h"
#include "pbrt/lexer/simd_lexer.h"
#include "pbrt/util/crack_atof.h"
#include "pbrt/util/crack_atof_avx2.h"
#include "pbrt/util/crack_atof_avx512.h"
#include "pbrt/util/crack_atof_sse.h"
#include "texture_cache.h"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <mio/mmap.hpp>
#include <mutex>
#include <pandora/graphics_core/load_from_file.h>
#include <pandora/graphics_core/pandora.h>
#include <pandora/graphics_core/perspective_camera.h>
#include <pandora/graphics_core/scene.h>
#include <pandora/textures/image_texture.h>
#include <stack>
#include <stream/cache/lru_cache.h>
#include <tbb/task_group.h>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>
#include <spdlog/spdlog.h>

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
    tasking::CacheBuilder* pGeometryCacheBuilder;

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

// Fully inlined / no error checking ring buffer makes the parser much faster than using a
// std::deque or even eastl::fixed_ring_buffer.
template <typename T, size_t N>
class RingBuffer {
public:
    inline void push_back(T v)
    {
        m_data[m_back] = v;
        m_back = (m_back + 1) % N;
        ++m_size;
    }
    inline T pop_front()
    {
        size_t index = m_front;
        m_front = (m_front + 1) % N;
        --m_size;
        return m_data[index];
    }

    inline size_t size() const
    {
        return m_size;
    }

    inline T operator[](size_t i) const
    {
        size_t index = (m_front + i) % N;
        return m_data[index];
    };

private:
    T m_data[N];
    size_t m_front { 0 };
    size_t m_back { 0 };
    size_t m_size { 0 };
};

class Parser {
public:
    Parser(std::filesystem::path basePath, unsigned subdiv = 0, bool loadTextures = true);

    pandora::RenderConfig parse(std::filesystem::path file, unsigned cameraID, tasking::CacheBuilder* pGeometryCacheBuilder = nullptr);

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
    const unsigned m_subdiv;
    const bool m_loadTextures;
    const std::filesystem::path m_basePath;

    std::stack<std::pair<std::shared_ptr<mio::mmap_source>, SIMDLexer>> m_lexerStack;
    std::shared_ptr<mio::mmap_source> m_pCurrentLexerSource;
    SIMDLexer m_currentLexer;
    //std::deque<Token> m_peekQueue;
    //eastl::fixed_ring_buffer<Token, 8> m_peekQueue { 8 };
    RingBuffer<Token, 8> m_peekQueue;

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

    //return static_cast<float>(crackAtof(tokenText));

    //return crack_atof_sse(tokenText);

#ifdef PBRT_ATOF_AVX512
    return crack_atof_avx512(tokenText);
#else
    // AVX2 port of crackAtof is even faster (WARNING: does not support exponential numbers (i.e. 10e5))
    return crack_atof_avx2(tokenText);
#endif
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
