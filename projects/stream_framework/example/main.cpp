#include "stream/pmr_allocator.h"
#include "stream/task_graph.h"
#include <EASTL/fixed_vector.h>
#ifdef  _WIN32
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

int main()
{
#ifdef  _WIN32
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);
#else
    auto colorLogger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("color_logger");
    spdlog::set_default_logger(colorLogger);
#endif

    struct Ray {
        int a;
        float b, c, d;
    };
    struct Hit {
        int a;
        float d, e, f;
    };

    tasking::TaskHandle<Ray> rayTask;
    tasking::TaskHandle<Hit> shadeTask;

    tasking::TaskGraph g;
    rayTask = g.addTask<Ray>(
        "rayTask",
		[&](gsl::span<const Ray> rays, std::pmr::memory_resource* pMemoryResource) {
        gsl::span<Hit> hits = tasking::allocateN<Hit>(pMemoryResource, rays.size());
        std::transform(std::begin(rays), std::end(rays), std::begin(hits),
            [](const Ray& ray) {
                return Hit { ray.a };
            });
        g.enqueue<Hit>(shadeTask, hits);
    });
    shadeTask = g.addTask<Hit>(
        "shadeTask",
        [&](gsl::span<const Hit> hits, std::pmr::memory_resource* pMemoryResource) {
        for (const Hit& hit : hits)
            spdlog::info("Hit: {}", hit.a);
    });

    std::vector<Ray> inputData;
    for (int i = 0; i < 100; i++)
        inputData.push_back(Ray { i });

    g.enqueue<Ray>(rayTask, inputData);
    g.run();

    return 0;
}
