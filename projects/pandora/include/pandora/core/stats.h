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

        metrics::Stopwatch<std::chrono::nanoseconds> svdagTraversalTime;
    } timings;

    struct {
        // Memory loaded by actual geometry (TriangleMesh) including during BVH construction
        metrics::Counter<size_t> geometryLoaded { "bytes" };
        metrics::Counter<size_t> geometryEvicted { "bytes" };

        // Memory loaded by geometry & bot-level BVHs
        metrics::Counter<size_t> botLevelLoaded { "bytes" };
        metrics::Counter<size_t> botLevelEvicted { "bytes" };
        metrics::Counter<size_t> botLevelTotalSize { "bytes" };

        // Memory used by the top-level BVH and the svdags associated with the top-level leaf nodes
        metrics::Counter<size_t> topBVH { "bytes" }; // BVH excluding leafs
        metrics::Counter<size_t> topBVHLeafs { "bytes" }; // leafs (without goemetry)
        metrics::Counter<size_t> batches { "bytes" };
        metrics::Counter<size_t> svdags { "bytes" };
    } memory;

    struct {
        std::string sceneFile;
        std::string integrator;
        int spp = 0;
    } config;

    metrics::Counter<size_t> numTopLevelLeafNodes { "nodes" };
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
