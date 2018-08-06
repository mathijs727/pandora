#include "pandora/integrators/svo_test_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/svo/voxel_grid.h"
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>

#undef PANDORA_ISPC_SUPPORT

namespace pandora {

template <typename OctreeType>
OctreeType buildSVO(const Scene& scene)
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

    return OctreeType(voxelGrid);
}

SVOTestIntegrator::SVOTestIntegrator(const Scene& scene, Sensor& sensor, int spp)
    : Integrator(scene, sensor, spp)
    , m_sparseVoxelOctree(std::move(buildSVO<OctreeType>(scene)))
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

			auto distanceOpt = m_sparseVoxelOctree.intersectScalar(ray);
			if (distanceOpt) {
				m_sensor.addPixelContribution(pixel, glm::vec3(std::max(0.0f, std::min(1.0f, *distanceOpt / 4.0f))));
			}
		}
}
#else

    constexpr static int simdBlockDim = 8;
    constexpr static int maxRayCount = simdBlockDim * simdBlockDim;
    tbb::blocked_range2d<int, int> sensorRange(0, resolution.y, 0, resolution.x);
    tbb::parallel_for(sensorRange, [&](tbb::blocked_range2d<int, int> localRange) {
        const auto& cols = localRange.cols();
        const auto& rows = localRange.rows();

		int threadBlockStartX = cols.begin();
		int threadBlockStartY = rows.begin();
		int threadBlockEndX = cols.end();
		int threadBlockEndY = rows.end();

#ifndef PANDORA_ISPC_SUPPORT // Scalar / SIMD
		for (int yi = threadBlockStartY; yi < threadBlockEndY; yi++) {
			for (int xi = threadBlockStartX; xi < threadBlockEndX; xi++) {
				auto pixel = glm::ivec2(xi, yi);
				CameraSample cameraSample = { pixel };
				Ray ray = camera.generateRay(cameraSample);

				auto distanceOpt = m_sparseVoxelOctree.intersectScalar(ray);
				if (distanceOpt) {
					m_sensor.addPixelContribution(pixel, glm::vec3(std::max(0.0f, std::min(1.0f, *distanceOpt / 4.0f))));
				}
			}
		}

#else // Scalar / SIMD
        for (int y = threadBlockStartY; y < threadBlockEndY; y += simdBlockDim) {
            for (int x = threadBlockStartX; x < threadBlockEndX; x += simdBlockDim) {
                int simdBlockStartX = x;
                int simdBlockEndX = std::min(x + simdBlockDim, cols.end());
                int simdBlockStartY = y;
                int simdBlockEndY = std::min(y + simdBlockDim, rows.end());

				/*std::array<float, maxRayCount> rayOriginX;
				std::array<float, maxRayCount> rayOriginY;
				std::array<float, maxRayCount> rayOriginZ;
				std::array<float, maxRayCount> rayDirectionX;
				std::array<float, maxRayCount> rayDirectionY;
				std::array<float, maxRayCount> rayDirectionZ;
				std::array<std::uint8_t, maxRayCount> hit;
				std::array<float, maxRayCount> hitT;
				int rayIdx = 0;
				for (int yi = simdBlockStartY; yi < simdBlockEndY; yi++) {
					for (int xi = simdBlockStartX; xi < simdBlockEndX; xi++) {
						auto pixel = glm::ivec2(xi, yi);
						CameraSample cameraSample = { pixel };
						Ray ray = camera.generateRay(cameraSample);
						rayOriginX[rayIdx] = ray.origin.x;
						rayOriginY[rayIdx] = ray.origin.y;
						rayOriginZ[rayIdx] = ray.origin.z;
						rayDirectionX[rayIdx] = ray.direction.x;
						rayDirectionY[rayIdx] = ray.direction.y;
						rayDirectionZ[rayIdx] = ray.direction.z;
						rayIdx++;
					}
				}*/

				int rayIdx = 0;
				std::array<CameraSample, maxRayCount> samples;
				for (int yi = simdBlockStartY; yi < simdBlockEndY; yi++) {
					for (int xi = simdBlockStartX; xi < simdBlockEndX; xi++) {
						samples[rayIdx++] = { glm::ivec2(xi, yi) };
					}
				}

				RaySOA<maxRayCount> soaRays;
				camera.generateRaySOA(samples, soaRays);

				ispc::RaySOA rays;
				rays.originX = soaRays.origin.x;
				rays.originY = soaRays.origin.y;
				rays.originZ = soaRays.origin.z;
				rays.directionX = soaRays.direction.x;
				rays.directionY = soaRays.direction.y;
				rays.directionZ = soaRays.direction.z;

				std::array<std::uint8_t, maxRayCount> hit;
				std::array<float, maxRayCount> hitT;
				ispc::HitSOA hits;
				hits.hit = hit.data();
				hits.t = hitT.data();

				m_sparseVoxelOctree.intersectSIMD(rays, hits, rayIdx);

				rayIdx = 0;
				for (int yi = simdBlockStartY; yi < simdBlockEndY; yi++) {
					for (int xi = simdBlockStartX; xi < simdBlockEndX; xi++) {
						auto pixel = glm::ivec2(xi, yi);

						if (hit[rayIdx]) {
							m_sensor.addPixelContribution(pixel, glm::vec3(std::max(0.0f, std::min(1.0f, hitT[rayIdx] / 4.0f))));
						}
						rayIdx++;
					}
				}
            }
        }
#endif // Scalar / SIMD
    });

#endif // Debug / release

    m_sppThisFrame += m_sppPerCall;
}

}
