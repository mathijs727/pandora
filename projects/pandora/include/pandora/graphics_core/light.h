#pragma once
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/ray.h"
#include "pandora/samplers/rng/pcg.h"
#include <glm/glm.hpp>
#include <optional>

namespace pandora {

struct LightSample {
    glm::vec3 radiance;

    glm::vec3 wi;
    float pdf;

    Ray visibilityRay;

    inline bool isBlack() const
    {
        return glm::dot(radiance, radiance) == 0.0f;
    }
};

enum class LightFlags : int {
    DeltaPosition = (1 << 0),
    DeltaDirection = (1 << 1),
    Area = (1 << 2),
    Infinite = (1 << 3)
};

class Light {
public:
    Light(int flags);
    virtual ~Light() {};

    bool isDeltaLight() const;

    //virtual glm::vec3 power() const = 0;
    virtual LightSample sampleLi(const Interaction& interaction, PcgRng& rng) const = 0;
    virtual float pdfLi(const Interaction& ref, const glm::vec3& wi) const = 0;

    virtual Spectrum Le(const Ray& w) const; // Radiance added to rays that miss the scene
protected:
    const int m_flags;
};

class InfiniteLight : public Light {
public:
    InfiniteLight(int flags);
};

}
