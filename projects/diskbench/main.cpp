#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mio/mmap.hpp>

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

template <typename F>
void testReadFunc(F&& f)
{
    using clock = std::chrono::high_resolution_clock;
    std::filesystem::path file = "D:\\Pandora Scenes\\pbrt_intermediate\\sanmiguel.zip";

    for (int i = 0; i < 10; i++) {
        std::vector<char> buffer(std::filesystem::file_size(file));

        auto start = clock::now();
        f(file, buffer);
        auto end = clock::now();
        auto diff = end - start;
        auto diffus = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        auto bytesRead = buffer.size();
        auto megaBytesPerSecond = bytesRead / diffus;
        std::cout << "Read speed: " << megaBytesPerSecond << "MB/s" << std::endl;
    }
}

int main()
{
    std::cout << "Mapped I/O" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        auto ro_mmap = mio::mmap_source(path.string(), 0, mio::map_entire_file);
        std::copy(std::begin(ro_mmap), std::end(ro_mmap), std::begin(ret));
    });

    std::cout << "\nMapped I/O (no buffering)" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        auto ro_mmap = mio::mmap_source(path.string(), 0, mio::map_entire_file, mio::no_buffering);
        std::copy(std::begin(ro_mmap), std::end(ro_mmap), std::begin(ret));
    });

    std::cout << "\nFile stream I/O" << std::endl;
    testReadFunc([](auto path, auto& ret) {
        readFileStream(path, ret);
    });

#ifdef _WIN32
    system("PAUSE");
#endif
    return 0;
}