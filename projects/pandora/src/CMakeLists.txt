target_sources(libPandora
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/core/path_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/perspective_camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/scene.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/interaction.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/sampler.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/geometry/bounds.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/geometry/triangle.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/lights/area_light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lights/environment_light.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/samplers/uniform_sampler.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/materials/lambert_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/mirror_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/shading.cpp"
        
        "${CMAKE_CURRENT_LIST_DIR}/textures/constant_texture.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/textures/image_texture.cpp"
		#"${CMAKE_CURRENT_LIST_DIR}/traversal/embree_accel.cpp"

        #"${CMAKE_CURRENT_LIST_DIR}/utility/blocked_stack_allocator.cpp"
)