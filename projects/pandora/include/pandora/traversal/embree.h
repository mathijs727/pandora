#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/traversal/ray.h"
#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#include <unordered_map>
#include <vector>

namespace pandora {

class EmbreeAccel {
public:
    EmbreeAccel(const std::vector<const Shape*>& geometry);
    ~EmbreeAccel();

    bool intersect(Ray& ray);

private:
    RTCDevice m_device;
    RTCScene m_scene;

    std::unordered_map<int, const Shape*> m_embreeGeomToShapeMapping;
};
}