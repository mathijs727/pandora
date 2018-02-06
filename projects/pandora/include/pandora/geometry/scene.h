#pragma once
#include <vector>
#include <gsl/span>

namespace pandora
{
class Shape;

class Scene// "Scene" clashes with an object defined in Embree (which does not use namespaces)
{
public:
	//Scene();
	//~Scene();

	// TODO: dont point to shapes but to SceneObjects
	void addShape(const Shape* shape);
	gsl::span<const Shape* const> getShapes() const;
private:
	std::vector<const Shape*> m_shapes;
};

}