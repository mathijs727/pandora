#include "pandora/core/progressive_renderer.h"
#include "pandora/core/path_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/shading/lambert_material.h"
#include "pandora/traversal/embree_accel.h"
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

ProgressiveRenderer::ProgressiveRenderer(int resolutionX, int resolutionY, const Scene& scene)
    : m_resolutionX(resolutionX)
    , m_resolutionY(resolutionY)
    , m_spp(1)
    , m_scene(scene)
{
}

void ProgressiveRenderer::clear()
{
    m_spp = 1;
}

void ProgressiveRenderer::incrementalRender(PerspectiveCamera& camera)
{
    camera.getSensor().clear(glm::vec3(0));
    PathIntegrator integrator(8, camera);
    integrator.render(m_scene);

    /*const float epsilon = 0.00001f;
    LambertMaterial material(glm::vec3(0.6f, 0.4f, 0.5f));

    float widthF = static_cast<float>(m_resolutionX);
    float heightF = static_cast<float>(m_resolutionY);
    tbb::parallel_for(0, m_resolutionY, [&](int y) {
        for (int x = 0; x < m_resolutionX; x++) {
            auto pixelRasterCoords = glm::ivec2(x, y);
            auto pixelScreenCoords = glm::vec2(x / widthF, y / heightF);
            Ray ray = camera.generateRay(CameraSample(pixelScreenCoords));
            glm::vec3 weight = glm::vec3(1.0f);

            int depth = 0;
            while (true) {
                IntersectionData intersection;
                m_accelerationStructure->intersect(ray, intersection);

                if (depth++ < 8 && intersection.objectHit != nullptr) {
                    auto shadingResult = material.sampleBSDF(intersection);
                    weight *= shadingResult.weight / shadingResult.pdf;
                    ray = Ray(intersection.position - epsilon * intersection.incident, shadingResult.out);
                    //m_sensor.addPixelContribution(pixelRasterCoords, glm::vec3(glm::dot(shadingResult.out, intersection.geometricNormal)));
                } else {
                    glm::vec3 backgroundColor = glm::vec3(1.0);
                    if (depth == 1)
                        m_sensor.addPixelContribution(pixelRasterCoords, glm::vec3(0));
                    else
                        m_sensor.addPixelContribution(pixelRasterCoords, backgroundColor * weight);

                    break;
                }
            }
        }
    });
    
    m_spp++;*/
}

int ProgressiveRenderer::getSampleCount() const
{
    return m_spp;
}

}
