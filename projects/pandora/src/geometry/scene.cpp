#include "pandora/geometry/scene.h"

namespace pandora
{

void SceneView::addShape(const Shape* shape)
{
	m_shapes.push_back(shape);
}

gsl::span<const Shape* const> SceneView::getShapes() const
{
	return m_shapes;
}

}
