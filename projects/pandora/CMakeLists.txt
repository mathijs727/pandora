add_library(libPandora "")
target_compile_features(libPandora PRIVATE cxx_std_17)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(libPandora PUBLIC "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS")
	target_compile_definitions(libPandora PUBLIC "NOMINMAX")

	# Surpess warnings from OpenImageIO & libmorton and enable AVX2
	set_target_properties(libPandora PROPERTIES COMPILE_FLAGS "/arch:AVX2 /wd5040 /wd4244 /wd4275 /wd4309")
endif()
enable_avx2(libPandora)
enable_popcnt(libPandora)
enable_bmi2(libPandora)

if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_definitions(libPandora PRIVATE "-D__TBB_CPP11_RVALUE_REF_PRESENT=1")
endif()

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
	assimp_target
    #assimp::assimp # Buggy (tries to link against a dll file...)
    OIIO::OpenImageIO # Loading textures
	glm # Vector math
    embree3::embree3 # BVH builder and reference BVH traversal
	#TBB::tbb # Multithreading
	#Threads::Threads
	eastl # Stack based STL containers <3 
	mio::mio) # Memory-mapped file IO for serialization

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
