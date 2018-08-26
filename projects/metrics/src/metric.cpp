#include "metrics/metric.h"
#include "metrics/exporter.h"
#include <chrono>
#include <nlohmann/json.hpp>

namespace metrics {

Metric::Metric(Identifier&& identifier, high_res_clock::time_point measurementStartTime)
    : m_identifier(std::move(identifier))
    , m_measurementStartTime(measurementStartTime)
{
}

void Metric::registerChangeListener(MessageQueue& messageQueue)
{
    m_listenerMessageQueue.push_back(&messageQueue);
}

void Metric::valueChanged(int newValue)
{
    Message message;
    message.identifier = m_identifier;
    message.timeStamp = getCurrentTimeStamp();
    message.content = ValueChangedMessage { newValue };
    for (auto& messageQueue : m_listenerMessageQueue) {
        messageQueue->push(message);
    }
}

TimeStamp Metric::getCurrentTimeStamp() const
{
    auto now = high_res_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - m_measurementStartTime);
}

}
