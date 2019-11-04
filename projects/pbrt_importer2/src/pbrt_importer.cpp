#include "pbrt/pbrt_importer.h"
#include "pbrt/parser/parser.h"

namespace pbrt {

pandora::RenderConfig loadFromPBRTFile(std::filesystem::path filePath, tasking::CacheBuilder* pCacheBuilder, bool loadTextures)
{
    std::filesystem::path basePath = filePath;
    basePath = basePath.remove_filename(); // Modified itself and returns a reference to *this

    Parser parser { basePath, loadTextures };
    return parser.parse(filePath, pCacheBuilder);
}

}