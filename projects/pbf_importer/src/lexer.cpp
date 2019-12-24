#include "pbf/lexer.h"
#include <exception>
#include <spdlog/spdlog.h>

// Binary file format lexer for PBF format by Ingo Wald.
// Based on code by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/semantic/BinaryFileFormat.cpp

namespace pbf {

Lexer::Lexer(std::filesystem::path filePath)
    : m_mappedFile(filePath.string(), mio_cache_control::cache_mode::sequential)
    , m_fileBytes(gsl::make_span(
          reinterpret_cast<const std::byte*>(m_mappedFile.data()), m_mappedFile.size()))
    , m_fileSize(m_mappedFile.size())
{
    if (filePath.extension() != ".pbf" && filePath.extension() != ".PBF") {
        spdlog::critical("Unsupported file extension \"{}\"", filePath.extension().string());
        throw std::runtime_error("");
    }
}

bool Lexer::endOfFile() const
{
    return m_cursor >= m_fileSize;
}

gsl::span<const std::byte> pbf::Lexer::readBytes(size_t numBytes)
{
    auto subSpan = m_fileBytes.subspan(m_cursor, numBytes);
    m_cursor += numBytes;
    return subSpan;
}

}