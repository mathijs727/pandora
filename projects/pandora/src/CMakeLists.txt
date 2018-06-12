target_sources(libPandora
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/core/path_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/perspective_camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/progressive_renderer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/scene.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/sensor.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/geometry/bounds.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/geometry/triangle.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/shading/lambert_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/shading/shading.cpp"
        
		"${CMAKE_CURRENT_LIST_DIR}/traversal/embree_accel.cpp"

        #"${CMAKE_CURRENT_LIST_DIR}/utility/blocked_stack_allocator.cpp"
)