#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"

namespace pandora {

void Scene::addMesh(std::shared_ptr<const TriangleMesh> mesh)
{
    m_meshes.push_back(mesh);
}

gsl::span<const std::shared_ptr<const TriangleMesh>> Scene::getMeshes() const
{
    return m_meshes;
}

}
