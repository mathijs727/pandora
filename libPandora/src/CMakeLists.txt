#include(${CMAKE_CURRENT_LIST_DIR}/bvh/CMakeLists.txt)

target_sources(libPandora
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/main.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/math/vec3.cpp"
)