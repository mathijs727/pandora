#pragma once
#include "pandora/core/pandora.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <mio/mmap.hpp>
#include <mutex>
#include <string_view>

// Use posix stuff so we can bypass the file caching on Linux
#ifdef __linux__
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <stdlib.h>
#endif

namespace pandora {

class DiskDataBatcher {
public:
    DiskDataBatcher(std::filesystem::path outFolder, std::string_view filePrefix, size_t maxFileSize)
        : m_outFolder(outFolder)
        , m_filePrefix(filePrefix)
        , m_maxFileSize(maxFileSize)
        , m_currentFileID(0)
        , m_currentFileSize(0)
    {
        openNewFile();
    }

#ifdef __linux__
    struct RAIIBuffer {
    public:
        RAIIBuffer(std::filesystem::path filePath, size_t offset, size_t size)
        {
            // Use Posix file I/O with O_DIRECT to bypass the operating system file cache
            int flags = O_RDONLY;
            if constexpr (OUT_OF_CORE_DISABLE_FILE_CACHING) {
                flags |= O_DIRECT;
            }
            auto fileDesc = open(filePath.string().c_str(), flags);
            ALWAYS_ASSERT(fileDesc >= 0);

            // Ensure that the offset and size are multiples of the block size
            constexpr int blockSize = 512; // Block size
            size_t offsetInBlock = offset % blockSize;
            size_t blockAlignedOffset = offset - offsetInBlock;
            size_t offsetAdjustedSize = size + offsetInBlock;

            size_t r = offsetAdjustedSize % blockSize;
            size_t bufferSize = r ? offsetAdjustedSize - r + blockSize : offsetAdjustedSize;

            m_raiiPtr.ptr = reinterpret_cast<std::byte*>(aligned_alloc(blockSize, bufferSize));
            lseek(fileDesc, blockAlignedOffset, SEEK_SET);
            ALWAYS_ASSERT(read(fileDesc, m_raiiPtr.ptr, bufferSize) >= 0);
            close(fileDesc);

            m_dataPtr = m_raiiPtr.ptr + offsetInBlock;
        }

        const void* data() const
        {
            return m_dataPtr;
        }

    private:
        struct RAIIPointer {
            ~RAIIPointer()
            {
                free(ptr);
            }
            std::byte* ptr;
        };
        RAIIPointer m_raiiPtr;
        void* m_dataPtr;
    };

#elif defined(WIN32)

    struct RAIIBuffer {
    public:
        RAIIBuffer(std::filesystem::path filePath, size_t offset, size_t size)
        {
            // Use Windows file I/O with FILE_FLAG_NO_BUFFERING to bypass the operating system file cache.
            // https://support.microsoft.com/en-us/help/99794/info-file-flag-write-through-and-file-flag-no-buffering
            int flags = 0;
            if constexpr (OUT_OF_CORE_DISABLE_FILE_CACHING) {
                flags |= FILE_FLAG_NO_BUFFERING;
            }
            auto fileHandle = CreateFile(
                filePath.string().c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                flags,
                nullptr);
            ALWAYS_ASSERT(fileHandle != INVALID_HANDLE_VALUE);

            // Ensure that the offset and size are multiples of the sector/block size
            constexpr int blockSize = 512; // Sector size
            size_t offsetInBlock = offset % blockSize;
            size_t blockAlignedOffset = offset - offsetInBlock;
            size_t offsetAdjustedSize = size + offsetInBlock;

            size_t r = offsetAdjustedSize % blockSize;
            size_t bufferSize = r ? offsetAdjustedSize - r + blockSize : offsetAdjustedSize;

            m_raiiPtr.ptr = reinterpret_cast<std::byte*>(_aligned_malloc(bufferSize, blockSize));
            DWORD bytesRead = 0;
            SetFilePointer(fileHandle, static_cast<DWORD>(blockAlignedOffset), 0, FILE_BEGIN);
            bool readSuccess = ReadFile(
                fileHandle,
                m_raiiPtr.ptr,
                static_cast<DWORD>(bufferSize),
                &bytesRead,
                nullptr);
            ALWAYS_ASSERT(readSuccess);
            //lseek(fileDesc, blockAlignedOffset, SEEK_SET);
            //ALWAYS_ASSERT(read(fileDesc, m_raiiPtr.ptr, bufferSize) >= 0);
            CloseHandle(fileHandle);

            m_dataPtr = m_raiiPtr.ptr + offsetInBlock;
        }

        const void* data() const
        {
            return m_dataPtr;
        }

    private:
        struct RAIIPointer {
            ~RAIIPointer()
            {
                _aligned_free(ptr);
            }
            std::byte* ptr;
        };
        RAIIPointer m_raiiPtr;
        void* m_dataPtr;
    };

#else
    struct RAIIBuffer {
    public:
        RAIIBuffer(std::filesystem::path filePath, size_t offset, size_t size)
        {
            static_assert(!OUT_OF_CORE_DISABLE_FILE_CACHING, "Disabling the file system cache is not supported on this platform");
            m_data = mio::mmap_source(filePath.string(), offset, size);
        }

        const void* data() const
        {
            return m_data.data();
        }

    private:
        mio::mmap_source m_data;
    };
#endif

    struct FilePart {
        // Shared between different file fragments to save memory
        std::shared_ptr<std::filesystem::path> filePathSharedPtr;
        size_t offset; // bytes
        size_t size; // bytes

        RAIIBuffer load() const
        {
            return RAIIBuffer(*filePathSharedPtr, offset, size);
        }
    };

    inline FilePart writeData(const flatbuffers::FlatBufferBuilder& fbb)
    {
        std::scoped_lock<std::mutex> l(m_accessMutex);

        FilePart ret = {
            m_currentFilePathSharedPtr,
            m_currentFileSize,
            fbb.GetSize()
        };
        m_file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
        m_currentFileSize += fbb.GetSize();

        if (m_currentFileSize > m_maxFileSize) {
            openNewFile();
        }
        assert(m_currentFileSize == m_file.tellp());
        return ret;
    }

    inline void flush()
    {
        m_file.flush();
    }

private:
    inline std::filesystem::path fullPath(int fileID)
    {
        using namespace std::string_literals;
        return m_outFolder / (m_filePrefix + std::to_string(fileID) + ".bin"s);
    }

    inline void openNewFile()
    {
        m_currentFilePathSharedPtr = std::make_shared<std::filesystem::path>(fullPath(++m_currentFileID));
        m_file = std::ofstream(*m_currentFilePathSharedPtr, std::ios::binary | std::ios::trunc);
        m_currentFileSize = 0;
    }

private:
    const std::filesystem::path m_outFolder;
    const std::string m_filePrefix;
    const size_t m_maxFileSize;

    std::mutex m_accessMutex;

    int m_currentFileID;
    std::shared_ptr<std::filesystem::path> m_currentFilePathSharedPtr;
    std::ofstream m_file;
    size_t m_currentFileSize;
};

}
