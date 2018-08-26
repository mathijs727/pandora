#include "metrics/metric.h"
#include "metrics/group.h"
#include "metrics/counter.h"
#include "metrics/offline_exporter.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace metrics;

int main()
{
    std::cout << "Hello world!" << std::endl;
    GroupBuilder statsBuilder;
    auto& memoryBuilder = statsBuilder.addGroup("memory");
    auto& memoryGeometryBuilder = memoryBuilder.addGroup("geometry");
    memoryGeometryBuilder.addCounter("mesh");
    memoryGeometryBuilder.addCounter("bvh");
    memoryGeometryBuilder.addCounter("dag");

    auto stats = statsBuilder.build();

    auto exporter = OfflineExporter(stats, "stats.json");

    auto& meshCounter = stats["memory"]["geometry"].get<Counter>("mesh");
    std::cout << "Mesh memory used: " << meshCounter << std::endl;
    meshCounter += 3;
    meshCounter += 1;
    meshCounter -= 2;
    std::cout << "Mesh memory used: " << meshCounter << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    system("pause");
}
