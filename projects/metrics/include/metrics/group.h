#pragma once
#include "metrics/counter.h"
#include <cassert>
#include <memory>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <chrono>

namespace metrics {

class Group {
public:
    const Group& operator[](std::string_view name) const;
    Group& operator[](std::string_view name);

    template <typename T>
    T& get(std::string_view name);
    template <typename T>
    const T& get(std::string_view name) const;

private:
    friend class Exporter;
    void registerMessageQueue(MessageQueue& messageQueue);

private:
    friend class GroupBuilder;
    using GroupPtr = std::unique_ptr<Group>; // Cant contain itself directly, but can contain a unique pointer to self
    using CounterPtr = std::unique_ptr<Counter>; // Cant contain itself directly, but can contain a unique pointer to self
    using Value = std::variant<GroupPtr, CounterPtr>;
    using ValueMap = std::unordered_map<std::string_view, Value>;
    Group(ValueMap&& valueMap);

private:
    ValueMap m_valueMap;
};

class GroupBuilder {
public:
    GroupBuilder() = default;

    GroupBuilder& addGroup(std::string_view name);
    void addCounter(std::string_view name);

    Group build() const;

private:
    using high_res_clock = std::chrono::high_resolution_clock;
    Group build(std::vector<std::string> nameHierarchy, high_res_clock::time_point constructionStartTime) const;

private:
    std::unordered_set<std::string> m_reservedNames;
    std::vector<std::string> m_counters;
    std::vector<std::pair<std::string, std::unique_ptr<GroupBuilder>>> m_children;
};

template <typename T>
inline T& Group::get(std::string_view name)
{
    using TPtr = std::unique_ptr<T>;
    assert(std::holds_alternative<TPtr>(m_valueMap[name]));
    return *std::get<TPtr>(m_valueMap[name]);
}

template <typename T>
inline const T& Group::get(std::string_view name) const
{
    using TPtr = std::unique_ptr<T>;
    assert(std::holds_alternative<TPtr>(m_valueMap[name]));
    return *std::get<TPtr>(m_valueMap[name]);
}

}
