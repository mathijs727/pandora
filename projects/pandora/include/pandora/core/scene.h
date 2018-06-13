#pragma once
#include <gsl/span>
#include <memory>
#include <vector>

namespace pandora {
class TriangleMesh;

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    // TODO: store scene objects with materials
    void addMesh(std::shared_ptr<const TriangleMesh> mesh);
    gsl::span<const std::shared_ptr<const TriangleMesh>> getMeshes() const;

private:
    std::vector<std::shared_ptr<const TriangleMesh>> m_meshes;
};

}
