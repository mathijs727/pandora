#pragma once
#include "stream/cache/evictable.h"
#include "stream/serialize/serializer.h"
#include <deque>
#include <filesystem>
#include <mio_cache_control/mmap.hpp>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <tbb/enumerable_thread_specific.h>

namespace tasking {

class SplitFileDeserializer : public Deserializer {
public:
    ~SplitFileDeserializer();
    SplitFileDeserializer(SplitFileDeserializer&&) = default;
    SplitFileDeserializer& operator=(SplitFileDeserializer&&) = default;

    const void* map(const Allocation& allocation) final;
    void unmap(const void* pMemory) final;

private:
    friend class SplitFileSerializer;
    SplitFileDeserializer(std::filesystem::path tempFolder, uint32_t numFiles, mio_cache_control::cache_mode fileCacheMode);

private:
    std::filesystem::path m_tempFolder;
    mio_cache_control::cache_mode m_fileCacheMode;

    std::mutex m_mutex;
    /*struct MappedFile {
        mio_cache_control::mmap_source file;
        uint32_t useCount;
    };
    std::unordered_map<uint32_t, MappedFile> m_openFiles;*/
    std::unordered_map<const void*, mio_cache_control::mmap_source> m_openFiles;
};

class SplitFileSerializer : public Serializer {
public:
    SplitFileSerializer(
        std::string_view folderName,
        size_t batchSize,
        mio_cache_control::cache_mode fileCacheMode = mio_cache_control::cache_mode::random_access);
    SplitFileSerializer(SplitFileSerializer&&) = default;
    SplitFileSerializer& operator=(SplitFileSerializer&&) = default;
    ~SplitFileSerializer();

    std::pair<Allocation, void*> allocateAndMap(size_t numBytes) final;
    void unmapPreviousAllocations() final;

    std::unique_ptr<Deserializer> createDeserializer() final;

private:
    void openNewFile(size_t minSize);

private:
    friend class SplitFileDeserializer;
    struct FileAllocation {
        size_t offsetInFile;
        size_t allocationSize;
        uint32_t fileID;
    };
    static_assert(sizeof(FileAllocation) <= sizeof(Allocation));

    std::filesystem::path m_tempFolder;
    mio_cache_control::cache_mode m_fileCacheMode;
    std::deque<mio_cache_control::mmap_sink> m_openFiles;

    size_t m_batchSize;
    size_t m_currentOffset { 0 };
    uint32_t m_currentFileID { 0 };
    mio_cache_control::mmap_sink m_currentFile;
};

}