#include "stream/static_data_cache.h"
#include <gtest/gtest.h>
#include <hpx/async.hpp>
#include <vector>

struct DummyResource {
    std::string path;

    size_t sizeBytes() const { return 1024; }
    static hpx::future<DummyResource> readFromDisk(std::filesystem::path p)
    {
        co_return DummyResource { p.string() };
    }
};

TEST(StaticDataCache, SingleThreadedRandomAccess)
{
    using namespace tasking;

    using Cache = VariableSizedResourceCache<DummyResource>;
    Cache cache { 1024 * 5, 100 };

    std::vector<Cache::ResourceID> resourceIDs;
    for (int i = 0; i < 10; i++)
        resourceIDs.push_back(cache.addResource(std::to_string(i)));

    std::random_device dev {};
    std::ranlux24 rng { dev() };
    std::uniform_int_distribution dist { 0, 9 };
    for (int i = 0; i < 5000; i++) {
        Cache::ResourceID resourceID = rng() % 10;
        auto resource = cache.lookUp(resourceID).get();
        ASSERT_EQ(resource->path, std::to_string(resourceID));
    }
}

TEST(StaticDataCache, MultiThreadedRandomAccess)
{
    using namespace tasking;

    using Cache = VariableSizedResourceCache<DummyResource>;
    Cache cache { 1024 * 75, 100 };

    std::vector<Cache::ResourceID> resourceIDs;
    for (int i = 0; i < 100; i++)
        resourceIDs.push_back(cache.addResource(std::to_string(i)));

    std::vector<hpx::future<void>> tasks;
    for (int i = 0; i < 10; i++) {
        tasks.push_back(hpx::async([&cache]() {
            std::random_device dev {};
            std::ranlux24 rng { dev() };
            std::uniform_int_distribution dist { 0, 99 };
            for (int i = 0; i < 5000; i++) {
                Cache::ResourceID resourceID = rng() % 100;

                auto resource = cache.lookUp(resourceID).get();
                ASSERT_EQ(resource->path, std::to_string(resourceID));
            }
        }));
    }

    for (auto& task : tasks)
        task.wait();
}

TEST(StaticDataCache, SingleThreadedLinearAccess)
{
    using namespace tasking;

    using Cache = VariableSizedResourceCache<DummyResource>;
    Cache cache { 1024 * 20, 10 };

    std::vector<Cache::ResourceID> resourceIDs;
    for (int i = 0; i < 100; i++)
        resourceIDs.push_back(cache.addResource(std::to_string(i)));

    for (const auto resourceID : resourceIDs) {
        auto resource = cache.lookUp(resourceID).get();
        ASSERT_EQ(resource->path, std::to_string(resourceID));
    }
}

TEST(StaticDataCache, MultiThreadedLinearAccess)
{
    using namespace tasking;

    using Cache = VariableSizedResourceCache<DummyResource>;
    Cache cache { 1024 * 100, 100 };

    std::vector<Cache::ResourceID> resourceIDs;
    for (int i = 0; i < 10000; i++)
        resourceIDs.push_back(cache.addResource(std::to_string(i)));

    std::vector<hpx::future<void>> tasks;
    for (int i = 0; i < 10000; i += 1000) {
        tasks.push_back(hpx::async([i, &cache]() {
            for (int resourceID = i; resourceID < i + 1000; resourceID++) {
                auto resource = cache.lookUp(resourceID).get();
                ASSERT_EQ(resource->path, std::to_string(resourceID));
            }
        }));
    }

    for (auto& task : tasks)
        task.wait();
}
