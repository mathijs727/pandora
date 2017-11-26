#pragma once
#include "pandora/math/vec3.h"
#include <vector>

namespace pandora {

class Sensor {
public:
    Sensor(int width, int height);

    void clear(Vec3f color);
    void addPixelContribution(int x, int y, Vec3f value);

private:
    int m_width, m_height;
    std::vector<Vec3f> m_frameBuffer;
};
}