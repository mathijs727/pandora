#include "metrics/metric.h"
#include <chrono>

namespace metrics {

Metric::Metric(Identifier&& identifier)
    : m_identifier(std::move(identifier))
{
}

void Metric::registerChangeListener(MessageQueue& messageQueue)
{
    m_listenerMessageQueue.push_back(&messageQueue);
}

void Metric::valueChanged(int newValue)
{
    for (auto& messageQueue : m_listenerMessageQueue) {
        messageQueue->push(ChangeMessage { m_identifier, newValue });
    }
}

}
