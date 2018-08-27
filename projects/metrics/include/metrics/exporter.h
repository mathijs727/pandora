#pragma once
#include <nlohmann/json.hpp>

namespace metrics
{

class Stats;
class Exporter {
protected:
    friend class Stats;
    virtual void notifyNewSnapshot(const nlohmann::json& snapshot) = 0;
};

}