#include "metrics/counter.h"
#include "metrics/metric.h"
#include "metrics/offline_exporter.h"
#include "metrics/stats.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace metrics;

struct MyStats : public metrics::Stats {
    // Inherit constructor
    using Stats::Stats;

    struct
    {
        Counter counter1;
        Counter counter2;
    } memory;
protected:
    nlohmann::json getMetricsSnapshot() const
    {
        nlohmann::json result;
        result["memory"]["counter1"] = memory.counter1;
        result["memory"]["counter2"] = memory.counter2;
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

    stats.memory.counter1 += 5;
    stats.memory.counter2 += 6;
    stats.asyncTriggerSnapshot();

    stats.memory.counter2 -= 2;
    stats.asyncTriggerSnapshot();

    // TODO: scoped timer (outputs time in constructor)
    // TODO: gauge (user sets value)
    // TODO: scoped label (to mark regions of time) with nesting (multiple overlapping labels)?

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    system("pause");
}
