target_sources(libPandora
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/core/bxdf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/interaction.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/material.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/perspective_camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/sampler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/spectrum.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/scene.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/core/sensor.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/geometry/bounds.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/geometry/triangle.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/integrators/path_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/naive_direct_lighting_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/direct_lighting_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/sampler_integrator.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/integrators/svo_test_integrator.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/lights/area_light.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lights/environment_light.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/samplers/rng/pcg.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/samplers/uniform_sampler.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/svo/mesh_to_voxel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_octree.cpp"
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
		
		#"${CMAKE_CURRENT_LIST_DIR}/traversal/bvh/embree_bvh_impl.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/utility/eastl_malloc.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/utility/memory_arena.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/utility/memory_arena_ts.cpp"
)

if (PANDORA_ISPC_SUPPORT)
	set(ISPC_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/")
	ADD_ISPC_LIBRARY(pandoraISPC STATIC
		"${CMAKE_CURRENT_LIST_DIR}/svo/mesh_to_voxel.ispc"
		"${CMAKE_CURRENT_LIST_DIR}/svo/sparse_voxel_octree_traversal.ispc")
	target_link_libraries(libPandora PUBLIC pandoraISPC)
endif()
