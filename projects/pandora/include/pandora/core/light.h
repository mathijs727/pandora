#pragma once
#include "glm/glm.hpp"
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
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
    Light(int flags, int numSamples = 1);

    bool isDeltaLight() const;

    //virtual glm::vec3 power() const = 0;
    virtual LightSample sampleLi(const Interaction& interaction, const glm::vec2& randomSample) const = 0;
	virtual float pdfLi(const Interaction& ref, const glm::vec3& wi) const = 0;

    virtual Spectrum Le(const Ray& w) const; // Radiance added to rays that miss the scene
protected:
    const int m_flags;
    const int m_numSamples;
};

class InfiniteLight : public Light {
public:
    InfiniteLight(int flags, int numSamples = 1);

    // PBRTv3 handles infinite area lights as a sphere around the scene. This means that they
    // "smear out" the light energy over the sphere (the larger the sphere the less the energy
    // any point on the sphere). This hack accounts for this by dividing the light intensity
    // by the area of the world sphere.
    virtual void divideIntensityByWorldSphereArea(float worldRadius) {}
};

}
