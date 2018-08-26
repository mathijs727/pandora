#pragma once
#include <tbb/concurrent_queue.h>
#include <thread>
#include <atomic>
#include "metrics/message_queue.h"

namespace metrics
{

class Group;
class Exporter {
public:
    Exporter(Group& group);
    ~Exporter();

protected:
    virtual void processMessage(const Message& message) = 0;

private:
    MessageQueue m_messageQueue;

    std::thread m_processingThread;
    std::atomic_bool m_shouldStop;
};

}