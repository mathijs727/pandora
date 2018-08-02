#pragma once
#include "pandora/core/pandora.h"
#include "pandora/core/integrator.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include <memory>

namespace pandora {

class SVOTestIntegrator : public Integrator<int>
{
public:
	// WARNING: do not modify the scene in any way while the integrator is alive
	SVOTestIntegrator(const Scene& scene, Sensor& sensor, int spp);

	void render(const PerspectiveCamera& camera) override final;

private:
	void rayHit(const Ray& r, SurfaceInteraction si, const int& s, const InsertHandle& h) override final {};
	void rayMiss(const Ray& r, const int& s) override final {};
private:
	SparseVoxelOctree m_sparseVoxelOctree;
};

}
