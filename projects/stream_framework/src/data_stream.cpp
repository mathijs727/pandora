#include "stream/data_stream.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace tasking {

DataStreamBlockImplWindows::DataStreamBlockImplWindows(size_t maxSize)
{
    // https://docs.microsoft.com/en-us/windows/desktop/fileio/creating-and-using-a-temporary-file
    // https://docs.microsoft.com/en-us/windows/desktop/memory/creating-a-file-mapping-object
    TCHAR lpTempPathBuffer[MAX_PATH];

    // Get the temp path env string (no guarantee it's a valid path).
    UINT dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
    if (dwRetVal > MAX_PATH || dwRetVal == 0) {
        std::cout << "Error: " << __FILE__ << ":" << __LINE__ << std::endl;
        exit(1);
    }

    // Generate a temporary file name.
    assert(m_fileName.size() >= MAX_PATH);
    UINT uRetval = GetTempFileName(lpTempPathBuffer, TEXT("STREAM"), 0, m_fileName.data());
    if (uRetval == 0) {
        std::cout << "Error: " << __FILE__ << ":" << __LINE__ << std::endl;
        exit(1);
    }

    m_fileHandle = CreateFile(
        (LPTSTR)m_fileName.data(),
        GENERIC_READ | GENERIC_WRITE, // open for write
        0, // do not share
        NULL, // default security
        CREATE_ALWAYS, // overwrite existing
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL); // no template
    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        std::cout << "Error: " << __FILE__ << ":" << __LINE__ << std::endl;
        exit(1);
    }

    m_fileMappingHandle = CreateFileMapping(
        m_fileHandle,
        NULL,
        PAGE_READWRITE,
        (maxSize >> 32) & 0xFFFFFFFF,
        maxSize & 0xFFFFFFFF,
        NULL);
    if (m_fileMappingHandle != 0)
        m_data = MapViewOfFile(m_fileMappingHandle, FILE_MAP_WRITE, 0, 0, maxSize);
}

DataStreamBlockImplWindows::DataStreamBlockImplWindows(DataStreamBlockImplWindows&& other) noexcept
    : m_data(other.m_data)
    , m_fileHandle(other.m_fileHandle)
    , m_fileMappingHandle(other.m_fileMappingHandle)
{
    std::copy(std::begin(other.m_fileName), std::end(other.m_fileName), std::begin(m_fileName));
    other.m_data = nullptr;
    other.m_fileHandle = nullptr;
    other.m_fileMappingHandle = nullptr;
}

DataStreamBlockImplWindows::~DataStreamBlockImplWindows()
{
    if (m_fileHandle) {
        assert(UnmapViewOfFile(m_data) != 0);
        CloseHandle(m_fileMappingHandle);
        CloseHandle(m_fileHandle);

        DeleteFile(m_fileName.data());
    }
}

}