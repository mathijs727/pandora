target_sources(libPandora
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/core/stats.cpp"

		"${CMAKE_CURRENT_LIST_DIR}/graphics_core/bounds.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/interaction.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/load_from_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/perspective_camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/sampler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/shape.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/spectrum.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/scene.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/graphics_core/transform.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/flatbuffers/data_conversion.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/shapes/tinyply.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/shapes/triangle_intersect.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/shapes/triangle_shading.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/shapes/triangle_shape.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/integrators/naive_direct_lighting_integrator.cpp"
        #"${CMAKE_CURRENT_LIST_DIR}/integrators/naive_direct_lighting_integrator_old.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/normal_debug_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/path_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/sampler_integrator.cpp"
        #"${CMAKE_CURRENT_LIST_DIR}/integrators/svo_depth_test_integrator.cpp"
        #"${CMAKE_CURRENT_LIST_DIR}/integrators/svo_test_integrator.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/lights/area_light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lights/distant_light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lights/environment_light.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/samplers/rng/pcg.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/samplers/uniform_sampler.cpp"

        #"${CMAKE_CURRENT_LIST_DIR}/scene/geometric_scene_object.cpp"
        #"${CMAKE_CURRENT_LIST_DIR}/scene/instanced_scene_object.cpp"

        #"${CMAKE_CURRENT_LIST_DIR}/svo/mesh_to_voxel.cpp"
        #"${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_dag.cpp"
        #"${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_octree.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/svo/voxel_grid.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/materials/matte_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/mirror_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/metal_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/plastic_material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/shading.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/materials/translucent_material.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/reflection/fresnel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/fresnel_blend_bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/lambert_bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/microfacet.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/microfacet_bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/oren_nayer_bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/scaled_bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/reflection/specular_bxdf.cpp"
        
        "${CMAKE_CURRENT_LIST_DIR}/textures/color_spaces.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/textures/constant_texture.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/textures/image_texture.cpp"
		
		"${CMAKE_CURRENT_LIST_DIR}/traversal/embree_acceleration_structure.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/traversal/batching_acceleration_structure.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/utility/eastl_malloc.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/utility/memory_arena.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/utility/thread_pool.cpp"
)

if (PANDORA_ISPC_SUPPORT)
	set(PANDORA_ISPC_ADDRESSING "32")
	set(ISPC_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/")
	ADD_ISPC_LIBRARY(pandoraISPC STATIC
		"${CMAKE_CURRENT_LIST_DIR}/svo/mesh_to_voxel.ispc"
		"${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_dag_traversal16.ispc"
		"${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_dag_traversal32.ispc"
		"${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_octree_traversal.ispc")
	target_link_libraries(libPandora PUBLIC pandoraISPC)
endif()
