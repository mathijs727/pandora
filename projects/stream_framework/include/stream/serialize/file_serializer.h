#pragma once
#include "stream/cache/evictable.h"
#include "stream/serialize/serializer.h"
#include <deque>
#include <filesystem>
#include <mio/mmap.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace stream {

class SplitFileDeserializer : public Deserializer {
public:
    ~SplitFileDeserializer();
    SplitFileDeserializer(SplitFileDeserializer&&) = default;
    SplitFileDeserializer& operator=(SplitFileDeserializer&&) = default;

    const void* map(const Allocation& allocation) final;
    void unmap(const Allocation& allocation) final;

private:
    friend class SplitFileSerializer;
    SplitFileDeserializer(std::filesystem::path tempFolder);

private:
    std::filesystem::path m_tempFolder;

    struct MappedFile {
        mio::mmap_source file;
        uint32_t useCount;
    };
    std::unordered_map<uint32_t, MappedFile> m_openFiles;
};

class SplitFileSerializer : public Serializer {
public:
    SplitFileSerializer(std::string_view folderName, size_t batchSize);
    SplitFileSerializer(SplitFileSerializer&&) = default;
    SplitFileSerializer& operator=(SplitFileSerializer&&) = default;
    ~SplitFileSerializer();

    std::pair<Allocation, void*> allocateAndMap(size_t numBytes) final;
    void unmapPreviousAllocations() final;

    std::unique_ptr<Deserializer> createDeserializer() final;

private:
    void openNewFile();

private:
    friend class SplitFileDeserializer;
    struct FileAllocation {
        uint32_t fileID;
        size_t offsetInFile;
    };
    static_assert(sizeof(FileAllocation) <= sizeof(Allocation));

    std::filesystem::path m_tempFolder;
    std::deque<mio::mmap_sink> m_openFiles;

    size_t m_batchSize;
    size_t m_currentOffset { 0 };
    uint32_t m_currentFileID { 0 };
    mio::mmap_sink m_currentFile;
};

}