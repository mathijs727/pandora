add_library(libPandora "")
target_compile_features(libPandora PRIVATE cxx_std_17)

if (MSVC)
    target_compile_definitions(libPandora PUBLIC "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS")
	target_compile_definitions(libPandora PUBLIC "NOMINMAX")

	# Surpess warnings from OpenImageIO & libmorton and enable AVX2
	set_target_properties(libPandora PROPERTIES COMPILE_FLAGS "/arch:AVX2 /wd5040 /wd4244 /wd4275 /wd4309")
else()
	#target_compile_options(libPandora PRIVATE -W -Wall -Wpedantic -Wno-unused-parameter -Wno-unused-variable -Wno-unused-private-field)
	target_compile_options(libPandora PUBLIC -mavx2)
endif()
target_compile_definitions(libPandora PRIVATE "ENABLE_STATIC_LIB")# Embree static lib

if (PANDORA_ISPC_SUPPORT)
	target_compile_definitions(libPandora PUBLIC "PANDORA_ISPC_SUPPORT")
endif()

set(pandora_flatbuffer_files
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/contiguous_allocator.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/triangle_mesh.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/wive_bvh8.fbs")
set(pandora_flatbuffer_include_folder "${CMAKE_CURRENT_LIST_DIR}/flatbuffers/include/")
build_flatbuffers(
	"${pandora_flatbuffer_files}" # flatbuffers_schemas
	"" # schema_include_dirs
	pandoraFlatbuffers # custom_target_name
	"" # additional_dependencies
	"${pandora_flatbuffer_include_folder}" # generated_includes_dir
	"" # binary_schemas_dir
	"") # copy_text_schemas_dir
add_dependencies(libPandora pandoraFlatbuffers)

target_link_libraries(libPandora PUBLIC
	simd
	metrics
	GSL
    assimp_target # Loading 3D models
    OpenImageIO # Loading textures
	glm # Vector math
    embree # BVH builder and reference BVH traversal
	TBB::tbb # Multithreading
	Threads::Threads
	EASTL # Stack based STL containers <3 
	mio) # Memory-mapped file IO for serialization

target_link_libraries(libPandora PRIVATE
	Boost::boost # Header-only Boost libraries
	libmorton) # Morton code for voxelization & SVO

if (UNIX)
	# Using atomic<vec3> in sensor.cpp requires atomic library on Unix
	target_link_libraries(libPandora PRIVATE atomic)
endif()

target_include_directories(libPandora PUBLIC
	"${CMAKE_CURRENT_LIST_DIR}/include/"
	"${pandora_flatbuffer_include_folder}")
target_include_directories(libPandora PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/include/CMakeLists.txt)

if (PANDORA_BUILD_TESTS)
    include(${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt)
endif()
