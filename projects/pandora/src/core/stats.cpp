#include "pandora/core/stats.h"
#include <vector>
#include <iostream>

namespace pandora {

metrics::OfflineExporter g_statsOfflineExporter { "stats.json" };
static auto exporters = std::vector<metrics::Exporter*>{ &g_statsOfflineExporter };
RenderStats g_stats(exporters);


nlohmann::json RenderStats::getMetricsSnapshot() const
{
    nlohmann::json result;
    result["memory"]["geometry"] = memory.geometry;
    result["memory"]["top_bvh"] = memory.topBVH;
    result["memory"]["bot_bvh"] = memory.botBVH;
    result["num_top_leaf_nodes"] = numTopLevelLeafNodes;
    for (const auto& flushInfo : flushInfos) {
        // clang-format off
        nlohmann::json flushInfoJSON{
            { "leafs_flushed", flushInfo.leafsFlushed.operator nlohmann::json() },
            { "batches_per_leaf", flushInfo.batchesPerLeaf.operator nlohmann::json()}
        };
        // clang-format on
        result["flush_info"].push_back(flushInfoJSON);
    }
    return result;
}

}
