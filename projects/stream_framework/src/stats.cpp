#include "stream/stats.h"

namespace tasking {

StreamStats& StreamStats::getSingleton()
{
    static metrics::OfflineExporter offlineExporter { "stream_stats.json" };
    static std::vector<metrics::Exporter*> exporters { &offlineExporter };
    static tasking::StreamStats stats { exporters };
    return stats;
}

StreamStats::~StreamStats()
{
    asyncTriggerSnapshot();
}

nlohmann::json StreamStats::getMetricsSnapshot() const
{
    nlohmann::json ret;

    for (const auto& flushInfo : infoAtFlushes) {
        ret["flushes"].push_back(toJSON(flushInfo));
    }

    return ret;
}

nlohmann::json StreamStats::toJSON(const GeneralStats& genStats)
{
    nlohmann::json ret;
    ret["queued_work_items"] = genStats.totalItemsInSystem;
    ret["queue_memory_usage"] = genStats.totalMemoryUsage;
    return ret;
}

nlohmann::json StreamStats::toJSON(const FlushInfo& flushInfo)
{
    nlohmann::json ret;
    ret["general_stats"] = toJSON(flushInfo.genStats);
    ret["items_flushed"] = flushInfo.itemsFlushed;
    ret["static_data_load_time"] = flushInfo.staticDataLoadTime;
    ret["processing_time"] = flushInfo.processingTime;
    return ret;
}

}