#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "pandora/core/ray.h"
#include "pandora/core/sensor.h"
#include "pandora/core/sampler.h"
#include <gsl/span>

namespace pandora {

class PerspectiveCamera {
public:
    PerspectiveCamera(glm::ivec2 resolution, float fovX, glm::mat4 transform = glm::mat4(1.0f));

    /*glm::vec3 getPosition() const;
    void setPosition(glm::vec3 pos);

    glm::quat getOrienation() const;
    void setOrientation(glm::quat orientation);*/
    glm::mat4 getTransform() const;
    void setTransform(const glm::mat4& m);

    Sensor& getSensor();
    glm::ivec2 getResolution() const;

	Ray generateRay(const CameraSample& sample) const;

	/*template <int N>
	void generateRaySOA(const gsl::span<CameraSample> samples, RaySOA<N>& outRays) const
	{
		for (int i = 0; i < samples.size(); i++) {
			const auto& sample = samples[i];
			glm::vec2 pixel2D = (sample.pixel / m_resolutionF - 0.5f) * m_virtualScreenSize;
			//glm::vec3 origin = m_position;
			glm::vec3 direction = glm::normalize(m_orientation * glm::vec3(pixel2D.x, pixel2D.y, 1.0f));

			outRays.origin.x[i] = m_position.x;
			outRays.origin.y[i] = m_position.y;
			outRays.origin.z[i] = m_position.z;

			outRays.direction.x[i] = direction.x;
			outRays.direction.y[i] = direction.y;
			outRays.direction.z[i] = direction.z;

			outRays.tnear[i] = 0.0f;
			outRays.tfar[i] = 1.0f;
		}
	}*/

private:
    Sensor m_sensor;
    glm::ivec2 m_resolution;
    glm::vec2 m_resolutionF;

    glm::vec2 m_virtualScreenSize;
    float m_aspectRatio;
    float m_fovX;

    glm::mat4 m_transform;
    /*glm::vec3 m_position;
    glm::quat m_orientation;*/
};
}
