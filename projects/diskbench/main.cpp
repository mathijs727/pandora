#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mio_cache_control/mmap.hpp>
#include <execution>

#ifdef __linux__
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef BLKPBSZGET
#define BLKSSZGET _IO(0x12, 104) /* get block device sector size */
#endif
#endif

void readFileStream(std::filesystem::path path, std::vector<char>& ret)
{
    // https://stackoverflow.com/questions/7241871/loading-a-file-into-a-vectorchar
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }
    file.seekg(0, std::ios_base::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios_base::beg);
    file.read(ret.data(), fileSize);
}

#ifdef __linux__
void readLinuxIO(std::filesystem::path path, std::vector<char>& ret, int flags)
{
    size_t fileSize = std::filesystem::file_size(path);

    int filedesc = open(path.string().c_str(), O_RDONLY | flags);
    if (filedesc < 0) {
        std::cout << "Fail to open file with errno: " << filedesc << std::endl;
        throw std::runtime_error("Failed to open file");
    }

    int alignment = 512; // Block size
    size_t r = fileSize % alignment;
    size_t bufferSize = r ? fileSize - r + alignment : fileSize;

    auto buffer = static_cast<char*>(aligned_alloc(alignment, bufferSize));
    int err = read(filedesc, buffer, bufferSize);
    if (err < 0) {
        std::cout << "Failed to read file with errno: " << err << std::endl;
    }

    memcpy(ret.data(), buffer, fileSize);

    free(buffer);
    close(filedesc);
}
#endif

template <typename F>
void testReadFunc(F&& f)
{
    using clock = std::chrono::high_resolution_clock;
    const std::filesystem::path file = "C:/Users/mathijs/Development/pbrt-v3-scenes/island/pbrt/isMountainA/xgBreadFruit/xgBreadFruit_archiveBreadFruitBaked_geometry.pbrt";

    float sumMegaBytesPerSec = 0;
    for (int i = 0; i < 10; i++) {
        // https://stackoverflow.com/questions/12721773/how-to-align-a-value-to-a-given-alignment
        size_t fileSize = std::filesystem::file_size(file);
        std::vector<char> buffer(fileSize);

        auto start = clock::now();
        f(file, buffer);
        auto end = clock::now();
        auto diff = end - start;
        auto diffus = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        auto bytesRead = buffer.size();
        auto megaBytesPerSecond = bytesRead / diffus;
        sumMegaBytesPerSec += megaBytesPerSecond;
        //std::cout << "Read speed: " << megaBytesPerSecond << "MB/s" << std::endl;

        size_t size = 0;
        for (char c : buffer)
            size += c;
        //std::cout << "Sum: " << size << std::endl;
    }

    const float avgMegaBytesPerSec = sumMegaBytesPerSec / 10;
    std::cout << "Average read speed: " << avgMegaBytesPerSec << "MB/s" << std::endl;
}

int main()
{
    std::cout << "Mapped I/O (random access)" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        auto ro_mmap = mio::mmap_source(path.string(), 0, mio::map_entire_file, mio::cache_mode::random_access);
        std::copy(std::begin(ro_mmap), std::end(ro_mmap), std::begin(ret));
    });

	std::cout << "\nMapped I/O (sequential access)" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        auto ro_mmap = mio::mmap_source(path.string(), 0, mio::map_entire_file, mio::cache_mode::sequential);
        std::copy(std::execution::par_unseq, std::begin(ro_mmap), std::end(ro_mmap), std::begin(ret));
    });

    std::cout << "\nMapped I/O (no buffering)" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        auto ro_mmap = mio::mmap_source(path.string(), 0, mio::map_entire_file, mio::cache_mode::no_buffering);
        //std::copy(std::execution::par_unseq, std::begin(ro_mmap), std::end(ro_mmap), std::begin(ret));
        std::memcpy(ret.data(), ro_mmap.data(), ro_mmap.size());
    });

    std::cout << "\nFile stream I/O" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        readFileStream(path, ret);
    });

#ifdef __linux__
    std::cout << "\nLinux file I/O (buffered)" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        readLinuxIO(path, ret, 0);
    });

    std::cout << "\nLinux file I/O (O_DIRECT)" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        readLinuxIO(path, ret, O_DIRECT);
    });
#endif

#ifdef _WIN32
    system("PAUSE");
#endif
    return 0;
}
