add_library(libPandora "")
target_compile_features(libPandora PRIVATE cxx_std_17)

if (WIN32)
	# Define both with MSVC and clang(-cl) on Windows.
	target_compile_definitions(libPandora PUBLIC "NOMINMAX")
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(libPandora PUBLIC "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS")

	# Surpess warnings from OpenImageIO & libmorton and enable AVX2
	set_target_properties(libPandora PROPERTIES COMPILE_FLAGS "/arch:AVX2 /wd5040 /wd4244 /wd4275 /wd4309")
	target_compile_options(libPandora PUBLIC "/wd4275")
endif()
enable_native_isa(libPandora)

target_compile_definitions(libPandora PRIVATE "ENABLE_STATIC_LIB")# Embree static lib

if (PANDORA_ISPC_SUPPORT)
	target_compile_definitions(libPandora PUBLIC "PANDORA_ISPC_SUPPORT")
endif()

set(pandora_flatbuffer_files
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/contiguous_allocator.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/data_types.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/ooc_batching.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/ooc_batching2.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/scene.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/triangle_mesh.fbs"
	"${CMAKE_CURRENT_LIST_DIR}/flatbuffers/wive_bvh8.fbs")
set(pandora_flatbuffer_include_folder "${CMAKE_CURRENT_LIST_DIR}/include/pandora/flatbuffers/")
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
	stream
	simd
	metrics
	assimp::assimp
    #assimp::assimp # Buggy (tries to link against a dll file...)
    OpenImageIO::OpenImageIO # Loading textures
	Cnl::Cnl # Fixed point math library
	glm # Vector math
    embree # BVH builder and reference BVH traversal
	TBB::tbb # Multithreading
	Threads::Threads
	EABase EASTL # Stack based STL containers <3 
	OptickCore
	mio::mio # Memory-mapped file IO for serialization
	mio_cache_control) # Modified version of mio that allows for control of the file system cache

target_link_libraries(libPandora PRIVATE Boost::boost)

if (LINK_STDCXX_FILESYSTEM)
	target_link_libraries(libPandora PRIVATE stdc++fs)
endif()
if (LINK_CXX_FILESYSTEM)
	target_link_libraries(libPandora PRIVATE c++fs)
endif()

if (UNIX)
	# Using atomic<vec3> in sensor.cpp requires atomic library on Unix
	target_link_libraries(libPandora PRIVATE atomic)
endif()

target_include_directories(libPandora PUBLIC
	"${CMAKE_CURRENT_LIST_DIR}/include/")
target_include_directories(libPandora PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/include/CMakeLists.txt)

#if (PANDORA_BUILD_TESTS)
#    include(${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt)
#endif()
