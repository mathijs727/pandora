#pragma once
#include "metrics/counter.h"
#include "metrics/gauge.h"
#include "metrics/histogram.h"
#include "metrics/offline_exporter.h"
#include "metrics/stats.h"
#include "metrics/stopwatch.h"
#include <chrono>
#include <list>

namespace pandora {

struct RenderStats : public metrics::Stats {
    // Inherit default constructor
    using metrics::Stats::Stats;

    struct {
        metrics::Stopwatch<std::chrono::milliseconds> totalRenderTime;
    } timings;

    struct {
        metrics::Counter<size_t> geometry { "bytes" };
        metrics::Counter<size_t> topBVH{ "bytes" };
        metrics::Counter<size_t> botBVH { "bytes" };
        //Counter dag { "Bytes" };
    } memory;

    metrics::Counter<> numTopLevelLeafNodes { "nodes" };
    struct FlushInfo {
        metrics::Counter<size_t> leafsFlushed { "nodes" };
        metrics::Histogram batchesPerLeaf { "batches", 1, 10, 11 };
    };
    std::list<FlushInfo> flushInfos;

protected:
    nlohmann::json getMetricsSnapshot() const override final;
};

extern metrics::OfflineExporter g_statsOfflineExporter;
extern RenderStats g_stats;

}
