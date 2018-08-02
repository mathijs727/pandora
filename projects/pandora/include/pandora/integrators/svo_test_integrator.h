#pragma once
#include "pandora/core/integrator.h"
#include "pandora/core/pandora.h"
#include <variant>

namespace pandora {

class SVOTestIntegrator : public Integrator<void>
{
public:
	// WARNING: do not modify the scene in any way while the integrator is alive
	SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp);

	void render(const PerspectiveCamera& camera) final;

protected:
	using RayState = void;
	virtual void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h) {};
	virtual void rayMiss(const Ray& r, const RayState& s) {};

	Ray spawnNextSample(const glm::vec2& pixel);

protected:
	const int m_maxDepth;
private:
	PerspectiveCamera const* m_cameraThisFrame;
};

}
