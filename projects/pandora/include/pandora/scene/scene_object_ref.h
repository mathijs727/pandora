#pragma once
#include "pandora/graphics_core/pandora.h"

namespace pandora {

struct SceneObjectRef {
    const Transform* pTransform;
    const SceneObject* pSceneObject;
};

}