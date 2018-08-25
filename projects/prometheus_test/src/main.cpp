#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <prometheus/exposer.h>
#include <prometheus/gateway.h>
#include <prometheus/registry.h>

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <sys/param.h>
#include <unistd.h>
#endif

static inline std::string GetHostName()
{
    std::array<char, 1024> hostname;

    if (gethostname(hostname.data(), (int)hostname.size())) {
        return {};
    }
    std::cout << "Hostname: " << std::string(hostname.data()) << std::endl;
    return std::string(hostname.data());
}

int main(int argc, char** argv)
{
    using namespace prometheus;

    // create an http server running on port 8080
    //Exposer exposer{ "127.0.0.1:8080" };

    const auto labels = Gateway::GetInstanceLabel("");
    
    Gateway gateway { "127.0.0.1:9091", "sample_client", labels };

    // create a metrics registry with component=main labels applied to all its
    // metrics
    auto registry = std::make_shared<Registry>();

    // add a new counter family to the registry (families combine values with the
    // same name, but distinct label dimenstions)
    auto& counter_family = BuildCounter()
                               .Name("time_running_seconds")
                               .Help("How many seconds is this server running?")
                               .Labels({ { "label", "value" } })
                               .Register(*registry);

    // add a counter to the metric family
    auto& second_counter = counter_family.Add(
        { { "another_label", "value" }, { "yet_another_label", "value" } });

    // ask the exposer to scrape the registry on incoming scrapes
    //exposer.RegisterCollectable(registry);
    gateway.RegisterCollectable(registry);

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // increment the counter by one (second)
        second_counter.Increment();

        // push metrics
        gateway.Push();
    }
    return 0;
}
