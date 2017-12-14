cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

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
include("${CMAKE_CURRENT_LIST_DIR}/external/tbb/tbb.cmake")

# Boost is not a CMake project and is pretty large. So let the user build/download it once.
#find_package(Boost)

#add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/GLEW/" EXCLUDE_FROM_ALL)
#add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/GSL/" EXCLUDE_FROM_ALL)
#add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/glfw/" EXCLUDE_FROM_ALL)
#add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/googletest/" EXCLUDE_FROM_ALL)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake")
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#set(GTEST_MSVC_SEARCH "MT")
#set(GTEST_ROOT "C:/Users/Mathijs/Documents/GitHub/pandora/third_party/install/GTest/")
find_or_install_package(glfw3)
find_or_install_package(GTest)
find_or_install_package(Boost)
find_or_install_package(GLEW)
install_header_only_package(GSL)

include(pandora/CMakeLists.txt)
include(atlas/CMakeLists.txt)
#include(torque/CMakeLists.txt)