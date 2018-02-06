#include "pandora/geometry/scene.h"

namespace pandora
{

void Scene::addShape(const Shape* shape)
{
	m_shapes.push_back(shape);
}

gsl::span<const Shape* const> Scene::getShapes() const
{
	return m_shapes;
}

}
