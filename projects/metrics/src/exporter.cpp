#include "metrics/exporter.h"
#include "metrics/group.h"

namespace metrics
{

Exporter::Exporter(Group& group) :
    m_shouldStop(false)
{
    group.registerMessageQueue(m_messageQueue);

    m_processingThread = std::thread([this]() {
        while (!m_shouldStop) {
            Message message;
            if (m_messageQueue.try_pop(message)) {
                processMessage(message);
            } else {
                // Queue is empty, wait some time before trying to pop again
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    });
}


Exporter::~Exporter()
{
    m_shouldStop = true;
    m_processingThread.join();
}

}