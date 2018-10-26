#pragma once
#include "pandora/core/pandora.h"
#include "pandora/core/integrator.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/svo/sparse_voxel_dag.h"

namespace pandora {

class SVOTestIntegrator : public Integrator<int>
{
public:
	// WARNING: do not modify the scene in any way while the integrator is alive
	SVOTestIntegrator(const Scene& scene, Sensor& sensor, int spp);

	void render(const PerspectiveCamera& camera) override final;

private:
	void rayHit(const Ray& r, SurfaceInteraction si, const int& s) override final {};
    void rayAnyHit(const Ray& r, const int& s) override final {};
	void rayMiss(const Ray& r, const int& s) override final {};
private:
	using OctreeType = SparseVoxelDAG;
	OctreeType m_sparseVoxelOctree;

    glm::mat4 m_worldToSVO;
    float m_svoToWorldScale;
};

}
