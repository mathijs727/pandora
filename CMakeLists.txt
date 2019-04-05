cmake_minimum_required(VERSION 3.11)
project(pandora)

option(PANDORA_ISPC_SUPPORT "Use ISPC to accelerate voxelization" OFF)
option(PANDORA_BUILD_TESTS "Build tests for Pandora" OFF)
option(PANDORA_BUILD_ATLAS "Build Atlas (real-time viewer)" ON)
option(PANDORA_BUILD_VOXEL_EXPORTER "Build voxel exporter" OFF)
option(PANDORA_BUILD_PBRT_IMPORTER "Build voxel exporter" OFF)
option(LINK_STDCXX_FILESYSTEM "Add libstdc++ filesystem to the linker flags" OFF)
option(LINK_CXX_FILESYSTEM "Add libc++ filesystem to the linker flags" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")
if (PANDORA_ISPC_SUPPORT)
	include("${CMAKE_CURRENT_LIST_DIR}/cmake/ispc.cmake")
endif()

find_package(OpenImageIO REQUIRED)
add_library(OpenImageIO INTERFACE)
target_include_directories(OpenImageIO INTERFACE "${OIIO_INCLUDE_DIR}")
target_link_libraries(OpenImageIO INTERFACE "${OIIO_OpenImageIO_LIBRARY}")

find_package(assimp REQUIRED)
add_library(assimp_target INTERFACE)
target_include_directories(assimp_target INTERFACE "${ASSIMP_INCLUDE_DIRS}")
target_link_libraries(assimp_target INTERFACE "${ASSIMP_LIBRARIES}")

find_package(embree 3 REQUIRED)
add_library(embree INTERFACE)
target_include_directories(embree INTERFACE "${EMBREE_INCLUDE_DIRS}")
target_link_libraries(embree INTERFACE "${EMBREE_LIBRARY}")

find_package(Threads REQUIRED)
find_package(TBB COMPONENTS tbb tbbmalloc tbbmalloc_proxy REQUIRED)

# Although these packages are shipped with vcpkg, they do not export any CMake files so we have to include
# them as subdirectory.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/ms-gsl/" EXCLUDE_FROM_ALL)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/eastl/" EXCLUDE_FROM_ALL)
add_library(libmorton INTERFACE)
target_include_directories(libmorton INTERFACE "${CMAKE_CURRENT_LIST_DIR}/third_party/libmorton/libmorton/include/")

# HPX does not use targets properly. HPX_INCLUDE_DIRS contains non-existent directories (error target_include_directories).
# So instead do it like this until HPX is updated to use modern CMake (it's 2019 guys...)
find_package(HPX CONFIG REQUIRED)
include_directories("${HPX_INCLUDE_DIRS}")
set_target_properties(hpx hpx_init iostreams_component
	PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)
if (MSVC)
	target_compile_definitions(hpx INTERFACE "_CRT_SECURE_NO_WARNINGS")
endif()
message("HPX_INCLUDE_DIRS: ${HPX_INCLUDE_DIRS}")

find_package(glm REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(nlohmann_json REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(mio CONFIG REQUIRED)

#if (PANDORA_BUILD_VOXEL_EXPORTER)
#	add_library(tinyply "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/tinyply.cpp")
#	target_include_directories(tinyply PUBLIC "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/")
#
#	add_subdirectory("projects/voxel/")
#endif()

find_package(Flatbuffers REQUIRED)
if (TARGET flatbuffers::flatc)
	get_property(FLATBUFFERS_FLATC_EXECUTABLE TARGET flatbuffers::flatc PROPERTY IMPORTED_LOCATION_RELEASE)
elseif (NOT DEFINED FLATBUFFERS_FLATC_EXECUTABLE)
	message(FATAL_ERROR "Please define FLATBUFFERS_FLATC_EXECUTABLE as the path to the flatbuffer executable")
endif()
include("cmake/BuildFlatBuffers.cmake") # Include cmake script that defines the flatc compile function

if (PANDORA_BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

if (PANDORA_BUILD_ATLAS)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

if (PANDORA_BUILD_TESTS)
	set(SIMD_BUILD_TESTS ON)
	set(STREAM_BUILD_TESTS ON)
endif()

enable_testing()

add_subdirectory("projects/stream_framework/")
add_subdirectory("projects/hpx_experiments/")
#include("projects/simd/CMakeLists.txt")
#include("projects/metrics/CMakeLists.txt")
#include("projects/pandora/CMakeLists.txt")
#if (PANDORA_BUILD_ATLAS)
#    include("projects/atlas/CMakeLists.txt")
#endif()
#include("projects/torque/CMakeLists.txt")

#add_subdirectory("projects/diskbench/")
#if (BUILD_VOXEL_EXPORT)
#    include("projects/voxel/CMakeLists.txt")
#endif()


#if (PANDORA_BUILD_PBRT_IMPORTER)
#	add_subdirectory("projects/pbrt_importer/")	
#endif()
