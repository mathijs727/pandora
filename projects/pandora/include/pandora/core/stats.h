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
        // Memory used by actual geometry (TriangleMesh)
        metrics::Counter<size_t> geometryLoaded { "bytes" };
        metrics::Counter<size_t> geometryEvicted { "bytes" };

        // Memory used by geometry & bot-level BVHs
        metrics::Counter<size_t> botLevelLoaded{ "bytes" };
        metrics::Counter<size_t> botLevelEvicted { "bytes" };

        metrics::Counter<size_t> topBVH { "bytes" };
    } memory;

    metrics::Counter<> numTopLevelLeafNodes { "nodes" };
    struct FlushInfo {
        metrics::Counter<size_t> leafsFlushed { "nodes" };
        metrics::Histogram batchesPerLeaf { "batches", 1, 10, 11 };
    };
    std::vector<FlushInfo> flushInfos;

protected:
    nlohmann::json getMetricsSnapshot() const override final;
};

extern metrics::OfflineExporter g_statsOfflineExporter;
extern RenderStats g_stats;

}
