#include "metrics/counter.h"
#include "metrics/gauge.h"
#include "metrics/histogram.h"
#include "metrics/metric.h"
#include "metrics/offline_exporter.h"
#include "metrics/stats.h"
#include "metrics/stopwatch.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace metrics;

struct MyStats : public metrics::Stats {
    // Inherit constructor
    using Stats::Stats;

    struct
    {
        Counter<> counter1 { "MB" };
        Counter<> counter2 { "MB" };
    } memory;

    Stopwatch<std::chrono::microseconds> stopwatch1;
    Gauge gauge1 { "MB" };

    Histogram histogram1 { "MB", 100, 200, 5 };

protected:
    nlohmann::json getMetricsSnapshot() const
    {
        nlohmann::json result;
        result["memory"]["counter1"] = memory.counter1;
        result["memory"]["counter2"] = memory.counter2;
        result["gauge1"] = gauge1;
        result["stopwatch1"] = stopwatch1;
        result["histogram"] = histogram1;
        return result;
    }
};

int main()
{
    std::cout << "Hello world!" << std::endl;

    auto offlineExporter = OfflineExporter("stats.json");
    std::vector<Exporter*> exporters = { &offlineExporter };
    MyStats stats(exporters);

    stats.asyncTriggerSnapshot();

    // Bin 1 & 2
    stats.histogram1.addItem(133);
    stats.histogram1.addItem(134);

    // Bin 0
    stats.histogram1.addItem(50);

    // Bin 5
    stats.histogram1.addItem(250);

    {
        auto stopwatch = stats.stopwatch1.getScopedStopwatch();
        stats.memory.counter1 += 5;
        stats.memory.counter2 += 6;
        stats.asyncTriggerSnapshot();

        stats.gauge1 = 42;
        stats.memory.counter2 -= 2;
    }
    stats.asyncTriggerSnapshot();

    // TODO: scoped timer (outputs time in constructor)
    // TODO: gauge (user sets value)
    // TODO: scoped label (to mark regions of time) with nesting (multiple overlapping labels)?

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
