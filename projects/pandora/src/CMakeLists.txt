#include(${CMAKE_CURRENT_LIST_DIR}/bvh/CMakeLists.txt)

target_sources(libPandora
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/core/perspective_camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/sensor.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/geometry/triangle.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/math/bounds3.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/mat3x4.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/quaternion.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/vec2.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/math/vec3.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/traversal/sbvh_builder.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/traversal/embree_builder.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/traversal/single_ray_traverser.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/traversal/test_bvh.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/utility/blocked_stack_allocator.cpp"
)