#include "pandora/core/progressive_renderer.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/scene.h"
#include "pandora/traversal/embree_accel.h"
#include <tbb/tbb.h>
#include <tbb/parallel_for.h>

namespace pandora {

ProgressiveRenderer::ProgressiveRenderer(int resolutionX, int resolutionY, const Scene& scene) :
	m_resolutionX(resolutionX), m_resolutionY(resolutionY), m_sensor(resolutionX, resolutionY),
	m_scene(scene),
	m_accelerationStructure(new EmbreeAccel(scene))
{
}

void ProgressiveRenderer::clear()
{
	m_sensor.clear(Vec3f(0.0f));
}

void ProgressiveRenderer::incrementalRender(const PerspectiveCamera& camera, int spp)
{
	float widthF = static_cast<float>(m_resolutionX);
	float heightF = static_cast<float>(m_resolutionY);
	tbb::parallel_for(0, m_resolutionY, [&](int y) {
		for (int x = 0; x < m_resolutionX; x++) {
			auto pixelRasterCoords = Vec2i(x, y);
			auto pixelScreenCoords = Vec2f(x / widthF, y / heightF);
			Ray ray = camera.generateRay(CameraSample(pixelScreenCoords));

			IntersectionData intersectionData;
			m_accelerationStructure->intersect(ray, intersectionData);
			if (intersectionData.objectHit != nullptr) {
				m_sensor.addPixelContribution(pixelRasterCoords, Vec3f(0.0f, intersectionData.uv.x, intersectionData.uv.y));
			}
		}
	});
}

const Sensor& ProgressiveRenderer::getSensor()
{
	return m_sensor;
}

}
