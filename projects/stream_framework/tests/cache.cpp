#include "stream/cache.h"
#include <cstddef>
#include <gsl/span>
#include <gtest/gtest.h>

struct GeometryData {
    int x;
    float y;

    size_t sizeBytes()
    {
        return sizeof(GeometryData);
    }

    std::vector<std::byte> serialize() const
    {
        auto start = reinterpret_cast<const std::byte*>(this);

        std::vector<std::byte> data;
        std::copy(start, start + sizeof(GeometryData), std::back_inserter(data));
        return data;
    }

    GeometryData(int x, float y)
        : x(x)
        , y(y)
    {
    }
    GeometryData(gsl::span<const std::byte> data)
    {
        const GeometryData* pGeomData = reinterpret_cast<const GeometryData*>(data.data());
        x = pGeomData->x;
        y = pGeomData->y;
    }

    /*static GeometryData load(void* pMem, gsl::span<const std::byte> data)
    {
        const GeometryData* pGeomData = reinterpret_cast<const GeometryData*>(data.data());
        return new GeometryData*pGeomData;
    }*/
};

TEST(LRUCache, UnlimitedMemory)
{
    stream::LRUCache<GeometryData>::Builder builder;
    auto handle1 = builder.registerCacheable(GeometryData { 3, 4.0f });
    auto handle2 = builder.registerCacheable(GeometryData { 9, 7.0f });
    auto handle3 = builder.registerCacheable(GeometryData { 4, 1.0f });

    auto cache = builder.build(1024);
    {
        auto pItem = cache.get(handle1);
        ASSERT_EQ(pItem->x, 3);
        ASSERT_EQ(pItem->y, 4.0f);
    }
    {
        auto pItem = cache.get(handle2);
        ASSERT_EQ(pItem->x, 9);
        ASSERT_EQ(pItem->y, 7.0f);
    }
    {
        auto pItem = cache.get(handle3);
        ASSERT_EQ(pItem->x, 4);
        ASSERT_EQ(pItem->y, 1.0f);
    }
}

TEST(LRUCache, Eviction)
{
    constexpr int range = 400;
    constexpr size_t memoryLimit = 128;

    std::vector<GeometryData> items;
    std::vector<stream::CacheHandle<GeometryData>> handles;

    stream::LRUCache<GeometryData>::Builder builder;
    for (int i = 0; i < range; i++) {
        GeometryData item { i, static_cast<float>(i * 2) };
        auto handle = builder.registerCacheable(item);

        handles.push_back(handle);
        items.push_back(item);
    }

    auto cache = builder.build(memoryLimit);

    for (int i = 0; i < range; i++) {
        auto cacheItem = cache.get(handles[i]);
        auto refItem = items[i];

        ASSERT_EQ(cacheItem->x, refItem.x);
        ASSERT_EQ(cacheItem->y, refItem.y);

        ASSERT_LE(cache.memoryUsage(), memoryLimit);
    }
}
