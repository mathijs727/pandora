cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

option(BUILD_TESTS "Build tests for Pandora" OFF)
option(BUILD_ATLAS "Build Atlas (real-time viewer)" ON)

# Download, compile and find dependencies
include("${CMAKE_CURRENT_LIST_DIR}/cmake/get_assimp.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/get_tbb.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake")

install_and_find_tbb()
find_or_install_package(Boost)
install_header_only_package(GSL)
find_or_install_assimp()
find_or_install_legacy_package(embree EMBREE_INCLUDE_DIRS EMBREE_LIBRARIES 2.17.1)
if (WIN32)
    # TODO: copy DLL
else()
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${CMAKE_CURRENT_LIST_DIR}/third_party/install/embree/lib")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${CMAKE_CURRENT_LIST_DIR}/third_party/install/GLEW/lib")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${CMAKE_CURRENT_LIST_DIR}/third_party/install/assimp/lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) # only if you want copy from the build tree
endif()

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