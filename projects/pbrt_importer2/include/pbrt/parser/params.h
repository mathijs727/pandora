#pragma once
#include <glm/glm.hpp>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

using ParamValue = std::variant<
    int,
    float,
    glm::vec2,
    glm::vec3,
    glm::mat4,
    std::string,
    std::vector<int>,
    std::vector<float>,
    std::vector<glm::vec2>,
    std::vector<glm::vec3>>;
class Params {
private:
    template <typename T>
    static constexpr bool holdsPossibleArray()
    {
        return std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3>;
    }

public:
    template <typename T>
    inline void add(std::string_view key, T&& value)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>)
            m_values[key] = std::string(value);
        else
            m_values[key] = std::forward<T>(value);
    }

    template <typename T>
    inline T&& getMove(std::string_view key)
    {
        if constexpr (holdsPossibleArray<T>()) {
            // PBRT has this weird format were it does not differentiate between scalars and arrays with length 1
            ParamValue v = m_values.find(key)->second;
            if (std::holds_alternative<T>(v))
                return std::move(std::get<T>(v));
            else
                return std::move(std::get<std::vector<T>>(v)[0]);
        } else {
            return std::move(std::get<T>(m_values.find(key)->second));
        }
    }
    template <typename T>
    inline T get(std::string_view key) const
    {
        if constexpr (holdsPossibleArray<T>()) {
            // PBRT has this weird format were it does not differentiate between scalars and arrays with length 1
            ParamValue v = m_values.find(key)->second;
            if (std::holds_alternative<T>(v))
                return std::get<T>(v);
            else
                return std::get<std::vector<T>>(v)[0];
        } else {
            if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>)
                return std::get<std::string>(m_values.find(key)->second);
            else
                return std::get<T>(m_values.find(key)->second);
        }
    }
    template <typename T>
    inline T get(std::string_view key, const T& defaultValue) const
    {
        if constexpr (holdsPossibleArray<T>()) {
            if (auto iter = m_values.find(key); iter != std::end(m_values)) {
                // PBRT has this weird format were it does not differentiate between scalars and arrays with length 1
                ParamValue v = iter->second;
                if (std::holds_alternative<T>(v))
                    return std::get<T>(v);
                else
                    return std::get<std::vector<T>>(v)[0];
            } else {
                return defaultValue;
            }
        } else {
            // PBRT has this weird format were it does not differentiate between scalars and arrays with length 1
            if (auto iter = m_values.find(key); iter != std::end(m_values)) {
                return std::get<T>(iter->second);
            } else {
                return defaultValue;
            }
        }
    }
    template <typename T>
    inline bool isOfType(std::string_view key) const
    {
        auto v = m_values.find(key)->second;
        if constexpr (holdsPossibleArray<T>()) {
            return std::holds_alternative<T>(v) || std::holds_alternative<std::vector<T>>(v);
        } else {
            return std::holds_alternative<T>(v);
        }
    }
    inline bool contains(std::string_view key) const
    {
        return (m_values.find(key) != std::end(m_values));
    }

private:
    std::unordered_map<std::string_view, ParamValue> m_values;
};