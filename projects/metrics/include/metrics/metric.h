#pragma once
#include "metrics/message_queue.h"
#include <chrono>
#include <vector>

namespace metrics {

class Metric {
public:
    using high_res_clock = std::chrono::high_resolution_clock;

public:
    Metric(Identifier&& identifier, high_res_clock::time_point measurementStartTime);

    void registerChangeListener(MessageQueue& messageQueue);

protected:
    void valueChanged(int newValue);

private:
    TimeStamp getCurrentTimeStamp() const;

private:
    Identifier m_identifier;
    high_res_clock::time_point m_measurementStartTime;
    std::vector<MessageQueue*> m_listenerMessageQueue;
};

}
