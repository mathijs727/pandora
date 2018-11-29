#include "pandora/core/stats.h"
#include "pandora/traversal/ooc_batching_acceleration_structure.h"
#include <iostream>
#include <vector>

namespace pandora {

metrics::OfflineExporter g_statsOfflineExporter { "stats.json" };
static auto exporters = std::vector<metrics::Exporter*> { &g_statsOfflineExporter };
RenderStats g_stats(exporters);

nlohmann::json RenderStats::getMetricsSnapshot() const
{
    nlohmann::json ret;
    ret["config"]["scene"] = config.sceneFile;
    ret["config"]["subdiv"] = SUBDIVIDE_LEVEL;
    ret["config"]["integrator"] = config.integrator;
    ret["config"]["spp"] = config.spp;

    ret["config"]["ooc"]["memory_limit_bytes"] = OUT_OF_CORE_MEMORY_LIMIT;
    ret["config"]["ooc"]["prims_per_leaf"] = OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF;
    ret["config"]["ooc"]["svdag_resolution"] = OUT_OF_CORE_SVDAG_RESOLUTION;

    ret["timings"]["total_render_time"] = timings.totalRenderTime;
    ret["timings"]["svdag_traversal_time"] = timings.svdagTraversalTime;

    ret["memory"]["geometry_loaded"] = memory.geometryLoaded;
    ret["memory"]["geometry_evicted"] = memory.geometryEvicted;

    ret["memory"]["bot_level_loaded"] = memory.botLevelLoaded;
    ret["memory"]["bot_level_evicted"] = memory.botLevelEvicted;
    ret["memory"]["ooc_total_disk_read"] = memory.oocTotalDiskRead;

    ret["memory"]["top_bvh"] = memory.topBVH;
    ret["memory"]["top_bvh_leafs"] = memory.topBVHLeafs;
    ret["memory"]["svdags_before_compression"] = memory.svdagsBeforeCompression;
    ret["memory"]["svdags_after_compression"] = memory.svdagsAfterCompression;

    ret["ooc"]["num_top_leaf_nodes"] = numTopLevelLeafNodes;
    ret["ooc"]["prims_per_leaf"] = OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF;
    ret["ooc"]["occlusion_culling"] = OUT_OF_CORE_OCCLUSION_CULLING;
    ret["ooc"]["file_caching_disabled"] = OUT_OF_CORE_DISABLE_FILE_CACHING;

    for (const auto& flushInfo : flushInfos) {
        // clang-format off
        nlohmann::json flushInfoJSON{
            { "approximate_rays_in_system", flushInfo.approximateRaysInSystem },
            { "num_batching_points_with_rays", flushInfo.numBatchingPointsWithRays },
            { "approximate_rays_per_flushed_batching_point", flushInfo.approximateRaysPerFlushedBatchingPoint },
            { "processing_time", flushInfo.processingTime.operator nlohmann::json() }
        };
        // clang-format on
        ret["flush_info"].push_back(flushInfoJSON);
    }

    ret["svdag"]["num_intersection_tests"] = svdag.numIntersectionTests;
    ret["svdag"]["num_rays_culled"] = svdag.numRaysCulled;

    return ret;
}

}
