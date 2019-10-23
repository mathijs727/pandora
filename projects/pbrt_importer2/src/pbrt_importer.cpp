#include "pbrt/pbrt_importer.h"
#include "pbrt/parser/parser.h"

namespace pbrt {

pandora::RenderConfig loadFromPBRTFile(std::filesystem::path filePath)
{
    std::filesystem::path basePath = filePath;
    basePath = basePath.remove_filename(); // Modified itself and returns a reference to *this

    Parser parser { basePath };
    return parser.parse(filePath);
}

}