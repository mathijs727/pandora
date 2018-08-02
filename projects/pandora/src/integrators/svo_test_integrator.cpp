#include "pandora/integrators/svo_test_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/svo/voxel_grid.h"
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>

namespace pandora {

SparseVoxelOctree buildSVO(const Scene& scene)
{
	Bounds gridBounds;
	for (const auto& sceneObject : scene.getSceneObjects()) {
		const auto& mesh = sceneObject->getMesh();
		gridBounds.extend(mesh.getBounds());
	}

	VoxelGrid voxelGrid(64);
	for (const auto& sceneObject : scene.getSceneObjects()) {
		const auto& mesh = sceneObject->getMesh();
		meshToVoxelGrid(voxelGrid, gridBounds, mesh);
	}

	return SparseVoxelOctree(voxelGrid);
}

SVOTestIntegrator::SVOTestIntegrator(const Scene& scene, Sensor& sensor, int spp)
    : Integrator(scene, sensor, spp)
    , m_sparseVoxelOctree(buildSVO(scene))
{
}

void SVOTestIntegrator::render(const PerspectiveCamera& camera)
{
    resetSamplers();

    // Generate camera rays
    glm::ivec2 resolution = m_sensor.getResolution();
#ifdef _DEBUG
    for (int y = 0; y < resolution.y; y++) {
        for (int x = 0; x < resolution.x; x++) {
			auto pixel = glm::ivec2(x, y);
			CameraSample cameraSample = { pixel };
			Ray ray = camera.generateRay(cameraSample);

			auto optDistance = m_sparseVoxelOctree.intersect(ray);
			if (optDistance) {
				m_sensor.addPixelContribution(pixel, glm::vec3(1.0f));
			}
        }
    }
#else
    tbb::blocked_range2d<int, int> sensorRange(0, resolution.y, 0, resolution.x);
    tbb::parallel_for(sensorRange, [&](tbb::blocked_range2d<int, int> localRange) {
        auto rows = localRange.rows();
        auto cols = localRange.cols();
        for (int y = rows.begin(); y < rows.end(); y++) {
            for (int x = cols.begin(); x < cols.end(); x++) {
                auto pixel = glm::ivec2(x, y);
                CameraSample cameraSample = { pixel };
                Ray ray = camera.generateRay(cameraSample);
				
				auto optDistance = m_sparseVoxelOctree.intersect(ray);
				if (optDistance) {
					m_sensor.addPixelContribution(pixel, glm::vec3(*optDistance));
				}
            }
        }
    });
#endif
    m_sppThisFrame += m_sppPerCall;
}

}
