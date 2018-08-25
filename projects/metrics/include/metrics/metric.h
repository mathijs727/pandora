#pragma once
#include "metrics/message_queue.h"
#include <vector>

namespace metrics {

class Metric {
public:
    Metric(Identifier&& identifier);

    void registerChangeListener(MessageQueue& messageQueue);

protected:
    void valueChanged(int newValue);

private:
    Identifier m_identifier;
    std::vector<MessageQueue*> m_listenerMessageQueue;
};

}
