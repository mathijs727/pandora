#include "stream/serialize/file_serializer.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace tasking {
SplitFileSerializer::SplitFileSerializer(std::string_view folderName, size_t batchSize, mio_cache_control::cache_mode fileCacheMode)
    : m_tempFolder(std::filesystem::temp_directory_path() / folderName)
    , m_fileCacheMode(fileCacheMode)
    , m_batchSize(batchSize)
{
    if (std::filesystem::exists(m_tempFolder)) {
        std::filesystem::remove_all(m_tempFolder);
    }
    spdlog::info("Creating temporary folder \"{}\"", m_tempFolder.string());
    std::filesystem::create_directories(m_tempFolder);
}

SplitFileSerializer::~SplitFileSerializer()
{
    m_openFiles.clear();
}

std::pair<Allocation, void*> SplitFileSerializer::allocateAndMap(size_t numBytes)
{
    assert(numBytes < m_batchSize);
    if (m_currentOffset + numBytes > m_batchSize || !m_currentFile.is_open() || !m_currentFile.is_mapped())
        openNewFile(numBytes);

    const size_t offset = m_currentOffset;
    m_currentOffset += numBytes;
    void* pMemory = reinterpret_cast<void*>(m_currentFile.data() + offset);

    FileAllocation fileAllocation { m_currentFileID, offset };
    Allocation allocation;
    std::memcpy(&allocation, &fileAllocation, sizeof(FileAllocation));

    return { allocation, pMemory };
}

void SplitFileSerializer::unmapPreviousAllocations()
{
    m_openFiles.clear();
}

std::unique_ptr<Deserializer> SplitFileSerializer::createDeserializer()
{
    // Cannot use make_unique with private constructors
    m_currentFile.unmap();
    return std::unique_ptr<SplitFileDeserializer>(new SplitFileDeserializer(m_tempFolder, m_currentFileID, m_fileCacheMode));
}

SplitFileDeserializer::SplitFileDeserializer(std::filesystem::path tempFolder, uint32_t maxFileID, mio_cache_control::cache_mode fileCacheMode)
    : m_tempFolder(tempFolder)
    , m_fileCacheMode(fileCacheMode)
{
    for (uint32_t fileID = 1; fileID <= maxFileID; fileID++) {
        const auto fileName = std::to_string(fileID) + ".bin";
        const auto filePath = m_tempFolder / fileName;
        auto mappedFile = mio_cache_control::mmap_source(filePath.string(), m_fileCacheMode);
        m_mappedFiles.push_back(std::move(mappedFile));
    }
}

SplitFileDeserializer::~SplitFileDeserializer()
{
    m_mappedFiles.clear();
    std::filesystem::remove_all(m_tempFolder);
}

const void* SplitFileDeserializer::map(const Allocation& allocation)
{
    SplitFileSerializer::FileAllocation fileAllocation;
    std::memcpy(&fileAllocation, &allocation, sizeof(decltype(fileAllocation)));

    const auto& file = m_mappedFiles[fileAllocation.fileID - 1];
    return reinterpret_cast<const void*>(file.data() + fileAllocation.offsetInFile);
}

void SplitFileDeserializer::unmap(const Allocation& allocation)
{
    /*SplitFileSerializer::FileAllocation fileAllocation;
    std::memcpy(&fileAllocation, &allocation, sizeof(decltype(fileAllocation)));

    if (auto iter = m_openFiles.find(fileAllocation.fileID); iter != std::end(m_openFiles)) {
        auto& mapped = iter->second;
        if (--mapped.useCount == 0)
            m_openFiles.erase(iter);
    } else {
        spdlog::error("SplitFileDeserializer trying to unmap a file that was not mapped");
    }*/
}

static void createFileOfSize(std::filesystem::path filePath, size_t size)
{
    // https://stackoverflow.com/questions/7896035/c-make-a-file-of-a-specific-size
    std::ofstream ofs { filePath, std::ios::binary | std::ios::out };
    ofs.seekp(size - 1);
    ofs.write("", 1);
}

void SplitFileSerializer::openNewFile(size_t minSize)
{
    if (m_currentFile.is_open() && m_currentFile.is_mapped())
        m_openFiles.push_back(std::move(m_currentFile));

    const std::filesystem::path fileName = std::to_string(++m_currentFileID) + ".bin";
    const auto filePath = m_tempFolder / fileName;

    createFileOfSize(filePath, std::max(m_batchSize, minSize));
    m_currentFile = mio_cache_control::mmap_sink(filePath.string(), mio_cache_control::cache_mode::random_access);
    m_currentOffset = 0;
}
}