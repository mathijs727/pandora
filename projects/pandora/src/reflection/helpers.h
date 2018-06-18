#pragma once
#include "glm/glm.hpp"
#include <algorithm>
#include <optional>

namespace pandora {

// These functions assume the shading coordinate system proposed in PBRTv3 with the normal vector always being (0,0,1)

// Theta: angle between vector and surface plane
inline float cosTheta(const glm::vec3& w){ return w.z; };
inline float cos2theta(const glm::vec3& w) { return w.z * w.z; }
inline float absCosTheta(const glm::vec3& w) { return std::abs(w.z); }

inline float sin2theta(const glm::vec3& w)
{
    return std::max(0.0f, 1.0f - cos2theta(w));
}
inline float sinTheta(const glm::vec3& w)
{
    return std::sqrt(sin2theta(w));
}

inline float tanTheta(const glm::vec3& w)
{
    return sinTheta(w) / cosTheta(w);
}
inline float tan2theta(const glm::vec3& w)
{
    return sin2theta(w) / cos2theta(w);
}

// Phi: angle between the vector projected onto the surface plane and the surface tangent
inline float cosPhi(const glm::vec3& w)
{
    float sinTheta1 = sinTheta(w);
    return (sinTheta1 == 0.0f) ? 1.0f : std::clamp(w.x / sinTheta1, -1.0f, 1.0f);
}

inline float sinPhi(const glm::vec3& w)
{
    float sinTheta1 = sinTheta(w);
    return (sinTheta1 == 0.0f) ? 0.0f : std::clamp(w.y / sinTheta1, -1.0f, 1.0f);
}

inline float cos2phi(const glm::vec3& w)
{
    return cosPhi(w) * cosPhi(w);
}

inline float sin2phi(const glm::vec3& w)
{
    return sinPhi(w) * sinPhi(w);
}

// Angle between two vectors when projected onto the surface plane
inline float cosDPhi(const glm::vec3& wa, const glm::vec3& wb)
{
    // Project onto plane by removing z coordinate. Angle is dot product of normalized 2D vectors.
    return std::clamp((wa.x * wb.x + wa.y * wb.y) /
        std::sqrt((wa.x * wa.x + wa.y * wa.y) * (wb.x * wb.x + wb.y*wb.y)), -1.0f, 1.0f);
}

inline glm::vec3 reflect(const glm::vec3& wo, glm::vec3& n)
{
    return -wo + 2 * glm::dot(wo, n) * n;
}

inline glm::vec3 faceForward(const glm::vec3& n, const glm::vec3& v) {
    return (glm::dot(n, v) < 0.0f) ? -n : n;
}

inline std::optional<glm::vec3> refract(const glm::vec3& wi, const glm::vec3& n, float eta)
{
    float cosThetaI = glm::dot(n, wi);
    float sin2thetaI = std::max(0.0f, 1.0f - cosThetaI * cosThetaI);
    float sin2thetaT = eta * eta * sin2thetaI;
    if (sin2thetaT)// Total internal reflection
        return {};
    float cosThetaT = std::sqrt(1 - sin2thetaT);
    return eta * -wi + (eta * cosThetaI - cosThetaT) * glm::vec3(n);
}

}
