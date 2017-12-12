cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

# Output to a folder called bin to prevent the following error:
# cannot open output file pandora: Is a directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


if (MSVC)
	# Google Test: Prevent overriding the parent project's compiler/linker settings on Windows
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING")
endif()

# Build TBB from source. This is the cleanest way to get TBB into a CMake project.
include("${CMAKE_CURRENT_LIST_DIR}/external/tbb/cmake/TBBBuild.cmake")
tbb_build(
    TBB_ROOT "${CMAKE_CURRENT_LIST_DIR}/external/tbb/"
    MAKE_ARGS "tbb_build_dir=${CMAKE_BINARY_DIR}/tbb/"
    CONFIG_DIR TBB_DIR)
find_package(TBB)

# Dont build docs & tests of GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Boost is not a CMake project and is pretty large. So let the user build/download it once.
find_package(Boost)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/GLEW/" EXCLUDE_FROM_ALL)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/GSL/" EXCLUDE_FROM_ALL)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/glfw/" EXCLUDE_FROM_ALL)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/googletest/" EXCLUDE_FROM_ALL)

include(pandora/CMakeLists.txt)
include(atlas/CMakeLists.txt)
include(torque/CMakeLists.txt)