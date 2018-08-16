#include "pandora/integrators/svo_depth_test_integrator.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/svo/mesh_to_voxel.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pandora {

SVODepthTestIntegrator::SVODepthTestIntegrator(const Scene& scene, Sensor& sensor, int spp) :
	SamplerIntegrator(2, scene, sensor, spp),
	m_svo(buildSVO(scene))
{
	Bounds gridBounds;
	for (const auto& sceneObject : scene.getSceneObjects()) {
		const auto& mesh = sceneObject->getMeshRef();
		gridBounds.extend(mesh.getBounds());
	}

	// SVO is at (1, 1, 1) to (2, 2, 2)
	float maxDim = maxComponent(gridBounds.extent());
	m_worldToSVO = glm::mat4(1.0f);
	m_worldToSVO = glm::translate(m_worldToSVO, glm::vec3(1.0f));
	m_worldToSVO = glm::scale(m_worldToSVO, glm::vec3(1.0f / maxDim));
	m_worldToSVO = glm::translate(m_worldToSVO, glm::vec3(-gridBounds.min));

	m_svoToWorldScale = maxDim;
}

void SVODepthTestIntegrator::rayHit(const Ray& ray, SurfaceInteraction si, const RayState& s, const InsertHandle& h)
{
	glm::ivec2 pixel = { 0, 0 };
	if (std::holds_alternative<ContinuationRayState>(s)) {
		const auto& rayState = std::get<ContinuationRayState>(s);
		pixel = rayState.pixel;
	} else if (std::holds_alternative<ShadowRayState>(s)) {
		const auto& rayState = std::get<ShadowRayState>(s);
		pixel = rayState.pixel;
	}

	Ray svoRay = ray;
	svoRay.origin = m_worldToSVO * glm::vec4(ray.origin, 1.0f);
	if (m_svo.intersectScalar(svoRay)) {
		m_sensor.addPixelContribution(pixel, glm::vec3(0, 1, 0));
	} else {
		m_sensor.addPixelContribution(pixel, glm::vec3(1, 0, 0));
	}
}

void SVODepthTestIntegrator::rayMiss(const Ray& ray, const RayState& s)
{
	glm::ivec2 pixel = { 0, 0 };
	if (std::holds_alternative<ContinuationRayState>(s)) {
		const auto& rayState = std::get<ContinuationRayState>(s);
		pixel = rayState.pixel;
	} else if (std::holds_alternative<ShadowRayState>(s)) {
		const auto& rayState = std::get<ShadowRayState>(s);
		pixel = rayState.pixel;
	}

	Ray svoRay = ray;
	svoRay.origin = m_worldToSVO * glm::vec4(ray.origin, 1.0f);
	if (m_svo.intersectScalar(svoRay)) {
		m_sensor.addPixelContribution(pixel, glm::vec3(0, 0, 1));
	}
}

SparseVoxelOctree SVODepthTestIntegrator::buildSVO(const Scene & scene)
{
	Bounds gridBounds;
	for (const auto& sceneObject : scene.getSceneObjects()) {
		const auto& mesh = sceneObject->getMeshRef();
		gridBounds.extend(mesh.getBounds());
	}

	VoxelGrid voxelGrid(64);
	for (const auto& sceneObject : scene.getSceneObjects()) {
		const auto& mesh = sceneObject->getMeshRef();
		meshToVoxelGrid(voxelGrid, gridBounds, mesh);
	}

	return SparseVoxelOctree(voxelGrid);
}

}
