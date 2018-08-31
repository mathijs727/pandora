#include <string>
#include <boost/python.hpp>
#include <memory>

struct World
{
	void set(std::string msg) { this->msg = msg; }
	std::string greet() { return this->msg; }
	std::string msg;
};

std::shared_ptr<World> createWorld()
{
	auto world = std::make_shared<World>();
	world->set("INITIALIZED");
	return world;
}

BOOST_PYTHON_MODULE(pandora_py)
{
    using namespace boost::python;
    class_<World>("World")
    	.def("greet", &World::greet)
    	.def("set", &World::set);

	register_ptr_to_python<std::shared_ptr<World>>();

	def("createWorld", createWorld);
}
