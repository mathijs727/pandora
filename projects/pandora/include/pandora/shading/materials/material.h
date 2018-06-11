#pragma once

namespace pandora {

class SurfaceInteraction;

class Material {
	virtual void computeScatteringFunctions(SurfaceInteraction& surfaceInteraction) const = 0;
};

}
