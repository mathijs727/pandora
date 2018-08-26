#pragma once
#include <string>
#include <tbb/concurrent_queue.h>
#include <vector>
#include <chrono>
#include <variant>

namespace metrics {

using Identifier = std::vector<std::string>;
using TimeStamp = std::chrono::duration<int, std::micro>;

struct ValueChangedMessage {
    int newValue;
};

struct Message {
    Identifier identifier;
    TimeStamp timeStamp;

    std::variant<ValueChangedMessage> content;
};

using MessageQueue = tbb::concurrent_queue<Message>;

}
