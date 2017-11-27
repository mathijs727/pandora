#include(${CMAKE_CURRENT_LIST_DIR}/bvh/CMakeLists.txt)

target_sources(libPandora
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/core/sensor.cpp"
        
        "${CMAKE_CURRENT_LIST_DIR}/math/mat3x4.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/quaternion.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/vec3.cpp"
)