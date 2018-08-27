#include "metrics/stats.h"
#include "metrics/exporter.h"
#include <chrono>

namespace metrics {

Stats::Stats(gsl::span<Exporter*> exporters)
    : m_shouldStop(false)
    , m_startTime(high_res_clock::now())
{
    std::copy(std::begin(exporters), std::end(exporters), std::back_inserter(m_exporters));

    m_processingThread = std::thread([this]() {
        while (!m_shouldStop) {
            nlohmann::json snapshot;
            while (m_workQueue.try_pop(snapshot)) {
                for (auto* exporter : m_exporters) {
                    exporter->notifyNewSnapshot(snapshot);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

Stats::~Stats()
{
    while (!m_workQueue.empty())
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    m_shouldStop = true;
    m_processingThread.join();
}

void Stats::asyncTriggerSnapshot()
{
    nlohmann::json snapshot;
    snapshot["data"] = getMetricsSnapshot();
    snapshot["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(high_res_clock::now() - m_startTime).count();
    m_workQueue.push(snapshot);
}

}
