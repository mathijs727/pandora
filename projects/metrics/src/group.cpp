#include "metrics/group.h"

namespace metrics {

Group::Group(ValueMap&& valueMap)
    : m_valueMap(std::move(valueMap))
{
}

Group& Group::operator[](std::string_view name)
{
    Value& v = m_valueMap[name];
    assert(std::holds_alternative<GroupPtr>(v));
    return *std::get<GroupPtr>(v);
}

void Group::registerMessageQueue(MessageQueue& messageQueue)
{
    for (auto& [name, value] : m_valueMap) {
        if (std::holds_alternative<GroupPtr>(value)) {
            std::get<GroupPtr>(value)->registerMessageQueue(messageQueue);
        } else if (std::holds_alternative<CounterPtr>(value)) {
            std::get<CounterPtr>(value)->registerChangeListener(messageQueue);
        }
    }
}

GroupBuilder& metrics::GroupBuilder::addGroup(std::string_view name)
{
    assert(m_reservedNames.find(std::string(name)) == m_reservedNames.end());
    m_reservedNames.emplace(name);

    auto child = std::make_unique<GroupBuilder>();
    auto* childPtr = child.get();
    m_children.emplace_back(name, std::move(child));
    return *childPtr;
}

void GroupBuilder::addCounter(std::string_view name)
{
    assert(m_reservedNames.find(std::string(name)) == m_reservedNames.end());
    m_reservedNames.emplace(name);

    m_counters.push_back(std::string(name));
}

Group GroupBuilder::build() const
{
    return build({}, high_res_clock::now());
}

Group GroupBuilder::build(std::vector<std::string> nameHierarchy, high_res_clock::time_point constructionStartTime) const
{
    Group::ValueMap valueMap;
    for (auto& [name, child] : m_children) {
        std::vector<std::string> childNameHierarchy = nameHierarchy;
        childNameHierarchy.push_back(name);

        auto group = std::make_unique<Group>(child->build(childNameHierarchy, constructionStartTime));
        valueMap[name] = std::move(group);
    }

    for (auto& name : m_counters) {
        std::vector<std::string> identifier = nameHierarchy;
        identifier.push_back(name);
        valueMap[name] = std::make_unique<Counter>(std::move(identifier), constructionStartTime);
    }

    // Can't use make_unique because we're using Group's private constructor
    return Group(std::move(valueMap));
}

}
