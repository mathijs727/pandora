#pragma once
#include "pandora/core/light.h"
#include "pandora/geometry/triangle.h"

namespace pandora {

class AreaLight : public Light {
public:
    AreaLight(glm::vec3 emittedLight, int numSamples, const TriangleMesh& mesh, unsigned primitiveID);

    glm::vec3 power() const final;

    glm::vec3 light(const Interaction& ref, const glm::vec3& w) const;
    LightSample sampleLi(const Interaction& ref, const glm::vec2& randomSample) const final;

private:
    const glm::vec3 m_emmitedLight;
    const TriangleMesh& m_mesh;
    const unsigned m_primitiveID;
    const float m_area;
};

std::vector<std::shared_ptr<AreaLight>> areaLightFromMesh(const TriangleMesh& mesh, const Spectrum& l);


}
