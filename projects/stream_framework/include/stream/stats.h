#pragma once
#include <metrics/exporter.h>
#include <metrics/gauge.h>
#include <metrics/offline_exporter.h>
#include <metrics/stats.h>
#include <metrics/stopwatch.h>
#include <spdlog/spdlog.h>

namespace tasking {

struct StreamStats : public metrics::Stats {
    // Inherit default constructor
    using metrics::Stats::Stats;

    struct GeneralStats {
        size_t totalItemsInSystem;
        size_t totalMemoryUsage;
    };
    struct FlushInfo {
        std::chrono::high_resolution_clock::time_point startTime;

        size_t itemsFlushed { 0 };
        metrics::Stopwatch<std::chrono::nanoseconds> staticDataLoadTime;
        metrics::Stopwatch<std::chrono::nanoseconds> processingTime;

        GeneralStats genStats;
    };
    std::vector<FlushInfo> infoAtFlushes;

public:
    static StreamStats& getSingleton();
    ~StreamStats();

protected:
    nlohmann::json getMetricsSnapshot() const override final;

private:
    static nlohmann::json toJSON(const GeneralStats& genStats);
    static nlohmann::json toJSON(const FlushInfo& flushInfo);
};

}
