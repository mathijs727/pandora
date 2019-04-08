#pragma once
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <hpx/async.hpp>
#include <hpx/lcos/local/condition_variable.hpp>
#include <hpx/lcos/local/mutex.hpp>
#include <mutex>
#include <optional>
#include <random>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/scalable_allocator.h>
#include <unordered_map>
#include <vector>

namespace tasking {

class Resource {
public:
    size_t sizeBytes() const;
    static hpx::future<Resource> readFromDisk();

private:
};

// Based on the (new) texture cache in PBRT:
// https://www.pbrt.org/texcache.pdf
template <typename T>
class VariableSizedResourceCache {
public:
    using ResourceID = uint32_t;

private:
    struct Entry {
        Entry(std::shared_ptr<T> ptr)
            : pointer(ptr)
        {
        }
        std::shared_ptr<T> pointer { nullptr };
        std::atomic_bool marked { false };
    };
    using HashMap = tbb::concurrent_unordered_map<ResourceID, Entry>;
    //using HashMap = std::unordered_map<ResourceID, Entry>;

public:
    VariableSizedResourceCache(size_t maxSizeBytes, size_t maxOccupancyEstimate)
        : m_threadActiveFlags(hpx::get_num_worker_threads())
        , m_markFreeSizeBytes(maxSizeBytes * 8 / 10)
        , m_maxSizeBytes(maxSizeBytes)
    {
        m_hashMap.store(new HashMap(4 * maxOccupancyEstimate));
        m_freeHashMap = new HashMap(4 * maxOccupancyEstimate);
    }

    ~VariableSizedResourceCache()
    {
        delete m_hashMap.load();
        delete m_freeHashMap;
    }

    ResourceID addResource(std::filesystem::path path)
    {
        m_resourceFactories.emplace_back(path);
        return static_cast<ResourceID>(m_resourceFactories.size() - 1);
    }

    hpx::future<std::shared_ptr<T>> lookUp(ResourceID resourceID) const
    {
        auto* mutThis = const_cast<VariableSizedResourceCache<T>*>(this);
        return mutThis->getResource(resourceID);
    }

private:
    hpx::future<std::shared_ptr<T>> getResource(ResourceID resourceID)
    {
        // Return resource if it's present in the hash table.
        {
            RCU rcu { m_threadActiveFlags };
            HashMap* hashMap = m_hashMap.load(std::memory_order_acquire);
            if (auto iter = hashMap->find(resourceID); iter != hashMap->end()) {
                std::shared_ptr<T> resource = iter->second.pointer;
                if (iter->second.marked.load(std::memory_order_relaxed))
                    iter->second.marked.store(false, std::memory_order_relaxed);
                co_return resource;
            }
        }

        // NOTE: possible race condition may occur where resource is not found but another thread
        //  just finished loading it before we start checking for outstanding reads. In that case
        //  this thread will try to load the same resource again. If this happens nothing bad will
        //  happen except for some extra disk I/O. Because of the low probability of this happening
        //  it is not worth checking again after acquiring the outstanding reads lock.

        // Check to see if another thread is already loading this resource.
        {
            m_outstandingReadsMutex.lock();
            for (ResourceID readResourceID : m_outstandingReads) {
                if (readResourceID == resourceID) {
                    // Wait for resourceID to be read before retrying lookup.
                    std::unique_lock readsLock { m_outstandingReadsMutex, std::adopt_lock };
                    m_outstandingReadsCondition.wait(readsLock);
                    readsLock.unlock();
                    co_return await getResource(resourceID);
                }
            }

            // Record that the current thread will read resourceID.
            m_outstandingReads.push_back(resourceID);
            m_outstandingReadsMutex.unlock();
        }

        // Load resource from disk.
        std::shared_ptr<T> resource = getFreeResource(await T::readFromDisk(m_resourceFactories[resourceID]));
        //m_resourceAllocator.construct(resource, await T::readFromDisk(m_resourceFactories[resourceID]));

        m_currentSizeBytes.fetch_add(resource->sizeBytes());

        // Add resource to hash table and return a pointer to it.
        {
            RCU rcu { m_threadActiveFlags };
            HashMap* hashMap = m_hashMap.load(std::memory_order_relaxed);
            hashMap->emplace(resourceID, resource);

            // Update outstanding reads for read resource.
            {
                std::scoped_lock lock { m_outstandingReadsMutex };
                for (auto iter = std::begin(m_outstandingReads); iter != std::end(m_outstandingReads); ++iter) {
                    if (*iter == resourceID) {
                        m_outstandingReads.erase(iter);
                        break;
                    }
                }
            }
            m_outstandingReadsCondition.notify_all();
        }

        co_return resource;
    }

    std::shared_ptr<T> getFreeResource(T&& resourceValue)
    {
        std::scoped_lock lock { m_freeResourcesMutex };

        size_t currentSizeBytes = m_currentSizeBytes.load(std::memory_order_relaxed);

        // Mark hash table entries if free-resource availability is low.
        if (currentSizeBytes > m_markFreeSizeBytes && !m_marked.load(std::memory_order_relaxed))
            markEntries();

        // Free resources when the cache is full.
        if (currentSizeBytes > m_maxSizeBytes)
            freeResources();

        return std::make_shared<T>(std::move(resourceValue)); //m_resourceAllocator.allocate(1);
    }

    void markEntries()
    {
        HashMap* hashMap = m_hashMap.load(std::memory_order_acquire);
        for (auto& [key, value] : *hashMap) {
            value.marked.store(true, std::memory_order_relaxed);
        }
        m_marked.store(true, std::memory_order_release);
    }

    void freeResources()
    {
        spdlog::info("VariableSizedResourceCache ({}) freeing resources", fmt::ptr(this));

        // Copy unmarked tiles to m_freeHashMap.
        {
            HashMap* hashMap = m_hashMap.load(std::memory_order_relaxed);
            m_freeHashMap->clear();

            size_t memoryFreedBytes = 0;
            for (const auto& [resourceID, entry] : *hashMap) {
                if (!entry.marked.load(std::memory_order_relaxed)) {
                    m_freeHashMap->emplace(resourceID, entry.pointer);
                } else {
                    // Not copied so the ref count is decreased and the resource will be freed once all accessing threads are done with it.
                    memoryFreedBytes += entry.pointer->sizeBytes();
                    //m_resourceAllocator.destroy(entry.pointer);
                    //m_resourceAllocator.deallocate(entry.pointer, 1);
                }
            }
            m_currentSizeBytes.fetch_sub(memoryFreedBytes);
            spdlog::info("VariableSizedResourceCache ({}) freed {} bytes, new size: {} bytes",
                fmt::ptr(this), memoryFreedBytes, m_currentSizeBytes.load(std::memory_order::memory_order_relaxed));

            // Handle case of all entries copied to m_freeHashMap
            if (memoryFreedBytes == 0) {
                spdlog::warn("VariableSizedResourceCache ({}) all resources are unmarked => evicting randomly!", fmt::ptr(this));
                m_freeHashMap->clear();

                // Copy random entries
                size_t memoryFreedBytes = 0;
                int i = 0;
                for (const auto& [resourceID, entry] : *hashMap) {
                    if (i++ % 2 == 0) {
                        m_freeHashMap->emplace(resourceID, entry.pointer);
                    } else {
                        // Not copied so the ref count is decreased and the resource will be freed once all accessing threads are done with it.
                        memoryFreedBytes += entry.pointer->sizeBytes();
                        //m_resourceAllocator.destroy(entry.pointer);
                        //m_resourceAllocator.deallocate(entry.pointer, 1);
                    }
                }
                m_currentSizeBytes.fetch_sub(memoryFreedBytes);

                spdlog::info("VariableSizedResourceCache ({}) freed {} bytes, new size: {} bytes",
                    fmt::ptr(this), memoryFreedBytes, m_currentSizeBytes.load(std::memory_order::memory_order_relaxed));
            }
        }

        // Swap resource cache hash maps.
        m_freeHashMap = m_hashMap.exchange(m_freeHashMap, std::memory_order_acq_rel);
        m_marked.store(false, std::memory_order_release);

        // Ensure that no threads are accessing the old hash map.
        for (size_t i = 0; i < m_threadActiveFlags.size(); i++) {
            waitForQuiescent(i);
        }

        // Add inactive resources in m_freeHashmap to free list.
        if (m_currentSizeBytes.load(std::memory_order_relaxed) > m_markFreeSizeBytes) {
            markEntries();
        }
    }

    /*// Begin read-side critical section.
    void RCUBegin()
    {
        std::atomic<bool>& flag = m_threadActiveFlags[hpx::get_worker_thread_num()].flag;
        flag.store(true, std::memory_order_relaxed);
        atomic_thread_fence(std::memory_order_acquire);
    }

    // End read-side critical section.
    void RCUEnd()
    {
        std::atomic<bool>& flag = m_threadActiveFlags[hpx::get_worker_thread_num()].flag;
        flag.store(false, std::memory_order_release);
    }*/

    void waitForQuiescent(size_t thread)
    {
        std::atomic<bool>& flag = m_threadActiveFlags[thread].flag;
        while (flag.load(std::memory_order_acquire) == true)
            ; // spin
    }

private:
    std::vector<std::filesystem::path> m_resourceFactories;

    std::atomic<HashMap*> m_hashMap;
    HashMap* m_freeHashMap;

    std::mutex m_freeResourcesMutex; // Do NOT use hpx::mutex because it will allow tasks to move threads (breaking RCU)
    tbb::scalable_allocator<T> m_resourceAllocator;
    const size_t m_markFreeSizeBytes;
    const size_t m_maxSizeBytes;
    std::atomic_bool m_marked { false };
    std::atomic_size_t m_currentSizeBytes { 0 };

    struct alignas(64) ActiveFlag {
        std::atomic<bool> flag { false };
    };
    std::vector<ActiveFlag> m_threadActiveFlags;

    // Read-side critical section
    struct RCU {
        RCU(std::vector<ActiveFlag>& flags)
            : m_flags(flags)
            , m_workerThreadID(hpx::get_worker_thread_num())
        {
            std::atomic<bool>& flag = flags[m_workerThreadID].flag;
            flag.store(true, std::memory_order_relaxed);
            atomic_thread_fence(std::memory_order_acquire);
        }

        ~RCU()
        {
            size_t workerID = hpx::get_worker_thread_num();
            assert(m_workerThreadID == workerID);
            std::atomic<bool>& flag = m_flags[m_workerThreadID].flag;
            flag.store(false, std::memory_order_release);
        }

    private:
        std::vector<ActiveFlag>& m_flags;
        size_t m_workerThreadID;
    };

    std::mutex m_outstandingReadsMutex; // Do NOT use hpx::mutex because it will allow tasks to move threads (breaking RCU)
    std::condition_variable m_outstandingReadsCondition;
    std::vector<ResourceID> m_outstandingReads;
};
}
