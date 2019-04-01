#include "stream/data_stream.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <fmt/format.h>
#include <fstream>
#include <spdlog/spdlog.h>

namespace tasking {

std::atomic_int s_dataStreamTempFileId{ 0 };

DataStreamChunkImpl::DataStreamChunkImpl(size_t maxSize)
{
    namespace fs = std::filesystem;

    fs::path tempDirPath = fs::current_path() / "tmp";
    if (!fs::exists(tempDirPath))
        fs::create_directories(tempDirPath);

    m_filePath = tempDirPath / fmt::format("stream_chunk{}.stream", s_dataStreamTempFileId.fetch_add(1));
    if (fs::exists(m_filePath))
        fs::remove(m_filePath);

    // Create file of given size.
    // https://stackoverflow.com/questions/7896035/c-make-a-file-of-a-specific-size
    std::ofstream ofs(m_filePath, std::ios::binary | std::ios::out);
    ofs.seekp(maxSize - 1);
    ofs.write("", 1);
    ofs.close();

    std::error_code error;
    m_memoryMapping = mio::make_mmap_sink(m_filePath.string(), 0, maxSize, error);
    if (error) {
        spdlog::critical("Failed to create data stream file \"{}\"", m_filePath.string());
        exit(1);
    }

    m_data = m_memoryMapping.data();
}

DataStreamChunkImpl::~DataStreamChunkImpl()
{
    namespace fs = std::filesystem;

    m_memoryMapping.unmap();
    fs::remove(m_filePath);
}


}
