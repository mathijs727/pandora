#pragma once
#include "pandora/traversal/ray.h"
#include <functional>
#include <gsl/span>

namespace pandora {

/*template <typename ShadingFunc, typename IntersectHandle>
class AccelerationStructure {
public:
    using RayWithShadeData = Ray;
    struct IntersectHandle {
    };

    using InsertShadingRay = std::function<void(const RayWithShadeData&)>;
    using ShadingFunc = std::function<void(const RayWithShadeData&, const SurfaceInteraction&, const InsertShadingRay&& ray)>;
    void setShadingFunc(ShadingFunc&& shadingFunc);

    virtual void placeIntersectRequests(gsl::span<const RayWithShadeData> rays) = 0;

protected:
    ShadingFunc m_shadingFunction;
};*/

}
