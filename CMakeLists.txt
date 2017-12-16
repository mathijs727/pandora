cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

if (NOT DEFINED BUILD_TESTS)
set(BUILD_TESTS OFF)
endif()
if (NOT DEFINED BUILD_ATLAS)
set(BUILD_ATLAS ON)
endif ()

# Dont build docs & tests of GLFW
#set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
#set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
#set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Dont build tests & tools of assimp
#set(ASSIMP_BUILD_TESTS OFF)
#set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)


# Building from source doesn't work on Windows natively (have to use minGW) because
# TBB still uses GNU make as build system. It is suprising how such an industry leading
# product relies on an outdated platform-specific tool instead of using CMake like
# everyone else.
#include("${CMAKE_CURRENT_LIST_DIR}/external/tbb/tbb.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/get_tbb.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake")
install_and_find_tbb()
find_or_install_package(Boost)
install_header_only_package(GSL)

if (BUILD_TESTS)
    find_or_install_package(GTest)
endif ()

if (BUILD_ATLAS)
    find_or_install_package(glfw3)
    find_or_install_package(GLEW)
    find_package(OpenGL)

    # GLEW generates a library called libglew32 but on Windows find_package(GLEW) looks for glew32s
    if (WIN32)
    	set(glew_lib_dir "${CMAKE_CURRENT_LIST_DIR}/third_party/install/GLEW/lib")
    	if (EXISTS "${glew_lib_dir}/libglew32.lib")
    		message("RENAMING ${glew_lib_dir}/libglew32.lib")
    		file(RENAME "${glew_lib_dir}/libglew32.lib" "${glew_lib_dir}/glew32.lib")
    	endif()
    	unset(glew_lib_dir)
    endif()
endif()

include("projects/pandora/CMakeLists.txt")
if (BUILD_ATLAS)
    include("projects/atlas/CMakeLists.txt")
endif()
#include("torque/CMakeLists.txt")