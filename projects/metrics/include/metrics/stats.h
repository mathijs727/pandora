#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <tbb/concurrent_queue.h>
#include <gsl/span>

namespace metrics
{

class Exporter;

class Stats
{
public:
    Stats(gsl::span<Exporter*> exporters);
    ~Stats();

    void asyncTriggerSnapshot();
protected:
    // I don't feel like writing an abstraction layer so just return the JSON directly.
    // This whole function wouldn't be necessary if C++ had compile time reflection (so we could loop over all member variables).
    virtual nlohmann::json getMetricsSnapshot() const = 0;
private:
    friend class Exporter;
    std::vector<Exporter*> m_exporters;

    // Worker thread
    std::atomic_bool m_shouldStop;
    std::thread m_processingThread;
    tbb::concurrent_queue<nlohmann::json> m_workQueue;

    // Time stamps
    using high_res_clock = std::chrono::high_resolution_clock;
    high_res_clock::time_point m_startTime;
};

}