#pragma once
#include <string>
#include <tbb/concurrent_queue.h>
#include <vector>

namespace metrics {

using Identifier = std::vector<std::string>;

struct ChangeMessage {
    Identifier identifier;
    int newValue;
};
using MessageQueue = tbb::concurrent_queue<ChangeMessage>;

}
