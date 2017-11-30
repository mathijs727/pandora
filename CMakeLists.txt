cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

# Output to a folder called bin to prevent the following error:
# cannot open output file pandora: Is a directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Google Test: Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING")
endif()

# Dont build docs & tests of GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/GSL/)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/GLEW/)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/glfw/)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/googletest/)

include(pandora/CMakeLists.txt)
include(atlas/CMakeLists.txt)