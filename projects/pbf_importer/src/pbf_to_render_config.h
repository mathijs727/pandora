#pragma once
#include <pandora/graphics_core/load_from_file.h>
#include "pbf/entities.h"

namespace pbf {

pandora::RenderConfig pbfToRenderConfig(PBFScene* pScene);

}