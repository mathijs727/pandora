#pragma once
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"
//#include <boost/multi_array.hpp>
#include <gsl/gsl>
#include <vector>
#include <memory>

namespace pandora {

class Sensor {
public:
    Sensor(int width, int height);

    void clear(Vec3f color);
    void addPixelContribution(Vec2i pixel, Vec3f value);

    int width() const { return m_width; }
    int height() const { return m_height; }
    //const FrameBufferConstArrayView getFramebufferView() const;
    gsl::not_null<const Vec3f*> getFramebufferRaw() const;
private:
    int getIndex(int x, int y) const;

private:
    int m_width, m_height;
    std::unique_ptr<Vec3f[]> m_frameBuffer;
};
}