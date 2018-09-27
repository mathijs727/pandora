#include "pandora/core/stats.h"
#include <vector>
#include <iostream>

namespace pandora {

metrics::OfflineExporter g_statsOfflineExporter { "stats.json" };
static auto exporters = std::vector<metrics::Exporter*>{ &g_statsOfflineExporter };
RenderStats g_stats(exporters);


nlohmann::json RenderStats::getMetricsSnapshot() const
{
    nlohmann::json ret;
    ret["timings"]["total_render_time"] = timings.totalRenderTime;

    ret["memory"]["geometry_loaded"] = memory.geometryLoaded;
    ret["memory"]["geometry_evicted"] = memory.geometryEvicted;

    ret["memory"]["bot_level_loaded"] = memory.botLevelLoaded;
    ret["memory"]["bot_level_evicted"] = memory.botLevelEvicted;

    ret["memory"]["top_bvh"] = memory.topBVH;
    ret["memory"]["top_bvh_leafs"] = memory.topBVHLeafs;
    ret["memory"]["svdags"] = memory.svdags;

    ret["num_top_leaf_nodes"] = numTopLevelLeafNodes;

    for (const auto& flushInfo : flushInfos) {
        // clang-format off
        nlohmann::json flushInfoJSON{
            { "leafs_flushed", flushInfo.leafsFlushed.operator nlohmann::json() },
            { "batches_per_leaf", flushInfo.batchesPerLeaf.operator nlohmann::json()}
        };
        // clang-format on
        ret["flush_info"].push_back(flushInfoJSON);
    }
    return ret;
}

}
