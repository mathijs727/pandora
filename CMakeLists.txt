unset(CMAKE_TOOLCHAIN_FILE)

cmake_minimum_required(VERSION 3.10)
project(pandora)

option(PANDORA_ISPC_SUPPORT "Use ISPC to accelerate voxelization" OFF)
option(PANDORA_BUILD_TESTS "Build tests for Pandora" OFF)
option(PANDORA_BUILD_ATLAS "Build Atlas (real-time viewer)" OFF)
option(PANDORA_BUILD_VOXEL_EXPORTER "Build voxel exporter" OFF)
option(PANDORA_BUILD_PBRT_IMPORTER "Build voxel exporter" OFF)
option(PANDORA_ISA_AVX512 "Compile code that uses AVX512" OFF)
option(LINK_STDCXX_FILESYSTEM "Add libstdc++ filesystem to the linker flags" OFF)
option(LINK_CXX_FILESYSTEM "Add libc++ filesystem to the linker flags" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/enable_simd.cmake")
if (PANDORA_ISPC_SUPPORT)
	include("${CMAKE_CURRENT_LIST_DIR}/cmake/ispc.cmake")
endif()

# vcpkg only installs Release and Debug libraries. Map RelWithDebInfo to Release.
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL MinSizeRel Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release)

find_package(assimp CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(EABase CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(mio CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(embree 3 CONFIG REQUIRED)
find_package(Cnl CONFIG REQUIRED)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/optick/" EXCLUDE_FROM_ALL)

get_target_property(EASTL_LOC EASTL IMPORTED_LOCATION_DEBUG)
message("EASTL LOC: ${EASTL_LOC}")

# Modified version of mio (https://github.com/mandreyel/mio) with a flag added to control the file system cache.
add_library(mio_cache_control INTERFACE)
target_include_directories(mio_cache_control INTERFACE "${CMAKE_CURRENT_LIST_DIR}/third_party/mio_cache_control/include/")

#if (PANDORA_BUILD_VOXEL_EXPORTER)
#	add_library(tinyply "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/tinyply.cpp")
#	target_include_directories(tinyply PUBLIC "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/")
#
#	add_subdirectory("projects/voxel/")
#endif()

find_package(Flatbuffers CONFIG REQUIRED)
if (TARGET flatbuffers::flatc)
	get_property(FLATBUFFERS_FLATC_EXECUTABLE TARGET flatbuffers::flatc PROPERTY IMPORTED_LOCATION_RELEASE)
elseif (NOT DEFINED FLATBUFFERS_FLATC_EXECUTABLE)
	message(FATAL_ERROR "Please define FLATBUFFERS_FLATC_EXECUTABLE as the path to the flatbuffer executable")
endif()
include("cmake/BuildFlatBuffers.cmake") # Include cmake script that defines the flatc compile function

if (PANDORA_BUILD_TESTS)
	enable_testing()
    find_package(GTest CONFIG REQUIRED)

	set(SIMD_BUILD_TESTS ON)
	set(STREAM_BUILD_TESTS ON)
endif()

if (PANDORA_BUILD_ATLAS)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

add_subdirectory("projects/diskbench/")
add_subdirectory("projects/stream_framework/")
include("projects/simd/CMakeLists.txt")
include("projects/metrics/CMakeLists.txt")
include("projects/pandora/CMakeLists.txt")
add_subdirectory("projects/pbf_importer/")
add_subdirectory("projects/pbrt_importer2/")
if (PANDORA_BUILD_ATLAS)
    include("projects/atlas/CMakeLists.txt")
endif()
include("projects/torque/CMakeLists.txt")


#if (BUILD_VOXEL_EXPORT)
#    include("projects/voxel/CMakeLists.txt")
#endif()
