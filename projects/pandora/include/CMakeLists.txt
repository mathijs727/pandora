target_sources(libPandora
PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/pandora/config.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/bounds.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/bxdf.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/interaction.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/light.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/load_from_file.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/material.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/pandora.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/perspective_camera.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/ray.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/sampler.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/scene.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/sensor.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/shape.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/spectrum.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/graphics_core/transform.h"

    # Files have to be generated before CMake can find them
	#"${CMAKE_CURRENT_LIST_DIR}/pandora/flatbuffers/contiguous_allocator_generated.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/flatbuffers/data_conversion.h"
    #"${CMAKE_CURRENT_LIST_DIR}/pandora/flatbuffers/ooc_batching_generated.h"
    #"${CMAKE_CURRENT_LIST_DIR}/pandora/flatbuffers/scene_generated.h"
    #"${CMAKE_CURRENT_LIST_DIR}/pandora/flatbuffers/triangle_mesh_generated.h"
    #"${CMAKE_CURRENT_LIST_DIR}/pandora/flatbuffers/wive_bvh8_generated.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/direct_lighting_integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/naive_direct_lighting_integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/normal_debug_integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/path_integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/sampler_integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/svo_depth_test_integrator.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/integrators/svo_test_integrator.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/lights/area_light.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/lights/distant_light.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/lights/environment_light.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/materials/matte_material.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/materials/metal_material.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/materials/mirror_material.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/materials/plastic_material.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/materials/translucent_material.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/samplers/rng/pcg.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/samplers/uniform_sampler.h"

    #"${CMAKE_CURRENT_LIST_DIR}/pandora/scene/scene_object_ref.h"
    #"${CMAKE_CURRENT_LIST_DIR}/pandora/scene/geometric_scene_object.h"
    #"${CMAKE_CURRENT_LIST_DIR}/pandora/scene/instanced_scene_object.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/shapes/forward_declares.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/shapes/group.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/shapes/template_magic.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/shapes/triangle.h"

    #"${CMAKE_CURRENT_LIST_DIR}/pandora/svo/mesh_to_voxel.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/svo/sparse_voxel_dag.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/svo/sparse_voxel_octree.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/svo/voxel_grid.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/textures/constant_texture.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/textures/image_texture.h"
    
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/embree_bvh_impl.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/embree_bvh.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/naive_single_bvh2.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/wive_bvh8_build2_impl.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/wive_bvh8_build2.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/wive_bvh8_build8_impl.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/wive_bvh8_build8.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/wive_bvh8_impl.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh/wive_bvh8.h"
    
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
    
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/embree_acceleration_structure.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/bvh.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/traversal/pauseable_bvh.h"

    "${CMAKE_CURRENT_LIST_DIR}/pandora/utility/error_handling.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/utility/math.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/utility/memory_arena.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/utility/memory_arena_ts.h"
    "${CMAKE_CURRENT_LIST_DIR}/pandora/utility/thread_pool.h"
)
