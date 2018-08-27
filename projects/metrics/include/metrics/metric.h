#pragma once
#include <nlohmann/json.hpp>

namespace metrics {

class Metric {
public:
    virtual operator nlohmann::json() const = 0;
};

}
