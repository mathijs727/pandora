#pragma once
#include "pandora/graphics_core/pandora.h"

namespace pandora {

struct SceneObjectRef {
    Transform* pTransform;
    SceneObject* pSceneObject;
};

}