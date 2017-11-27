#pragma once
#include "pandora/math/vec3.h"
#include <gsl/gsl>
#include <vector>

namespace pandora {

class Sensor {
public:
    Sensor(int width, int height);

    void clear(Vec3f color);
    void addPixelContribution(int x, int y, Vec3f value);

    int width() const { return m_width; }
    int height() const { return m_height; }
    gsl::multi_span<const Vec3f, gsl::dynamic_range, gsl::dynamic_range> getFramebuffer() const;

private:
    int m_width, m_height;
    std::vector<Vec3f> m_frameBuffer;
};
}