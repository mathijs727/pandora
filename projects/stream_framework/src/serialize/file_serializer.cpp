#include "stream/serialize/file_serializer.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace tasking {
SplitFileSerializer::SplitFileSerializer(std::string_view folderName, size_t batchSize)
    : m_tempFolder(std::filesystem::temp_directory_path() / folderName)
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
        openNewFile();

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
    return std::unique_ptr<SplitFileDeserializer>(new SplitFileDeserializer(m_tempFolder));
}

SplitFileDeserializer::SplitFileDeserializer(std::filesystem::path tempFolder)
    : m_tempFolder(tempFolder)
{
}

SplitFileDeserializer::~SplitFileDeserializer()
{
    m_openFiles.clear();
    std::filesystem::remove_all(m_tempFolder);
}

const void* SplitFileDeserializer::map(const Allocation& allocation)
{
    SplitFileSerializer::FileAllocation fileAllocation;
    std::memcpy(&fileAllocation, &allocation, sizeof(decltype(fileAllocation)));

    if (auto iter = m_openFiles.find(fileAllocation.fileID); iter != std::end(m_openFiles)) {
        auto& mapped = iter->second;
        mapped.useCount++;
        return reinterpret_cast<const void*>(mapped.file.data() + fileAllocation.offsetInFile);
    } else {
        const auto fileName = std::to_string(fileAllocation.fileID) + ".bin";
        const auto filePath = m_tempFolder / fileName;
        auto mappedFile = mio::mmap_source(filePath.string());
        const void* pResult = mappedFile.data() + fileAllocation.offsetInFile;

        m_openFiles.emplace(std::pair { fileAllocation.fileID, MappedFile { std::move(mappedFile), 0 } });
        return pResult;
    }
}

void SplitFileDeserializer::unmap(const Allocation& allocation)
{
    SplitFileSerializer::FileAllocation fileAllocation;
    std::memcpy(&fileAllocation, &allocation, sizeof(decltype(fileAllocation)));

    if (auto iter = m_openFiles.find(fileAllocation.fileID); iter != std::end(m_openFiles)) {
        auto& mapped = iter->second;
        if (--mapped.useCount == 0)
            m_openFiles.erase(iter);
    } else {
        spdlog::error("SplitFileDeserializer trying to unmap a file that was not mapped");
    }
}

static void createFileOfSize(std::filesystem::path filePath, size_t size)
{
    // https://stackoverflow.com/questions/7896035/c-make-a-file-of-a-specific-size
    std::ofstream ofs { filePath, std::ios::binary | std::ios::out };
    ofs.seekp(size - 1);
    ofs.write("", 1);
}

void SplitFileSerializer::openNewFile()
{
    if (m_currentFile.is_open() && m_currentFile.is_mapped())
        m_openFiles.push_back(std::move(m_currentFile));

    const std::filesystem::path fileName = std::to_string(++m_currentFileID) + ".bin";
    const auto filePath = m_tempFolder / fileName;

    createFileOfSize(filePath, m_batchSize);
    m_currentFile = mio::mmap_sink(filePath.string());
    m_currentOffset = 0;
}
}