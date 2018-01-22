cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

option(BUILD_TESTS "Build tests for Pandora" OFF)
option(BUILD_ATLAS "Build Atlas (real-time viewer)" ON)

# Download, compile and find dependencies
include("${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/get_assimp.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/get_tbb.cmake")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(THIRD_PARTY_INSTALL_FOLDER "${CMAKE_CURRENT_LIST_DIR}/third_party/install/debug/")
else()
    set(THIRD_PARTY_INSTALL_FOLDER "${CMAKE_CURRENT_LIST_DIR}/third_party/install/release/")
endif()
install_and_find_tbb()
find_or_install_package(Boost)
install_header_only_package(GSL)
find_or_install_assimp()
find_or_install_legacy_package(embree EMBREE_INCLUDE_DIRS EMBREE_LIBRARIES 2.17.1)
if (WIN32)
    # TODO: copy DLL
	file(COPY "${THIRD_PARTY_INSTALL_FOLDER}/embree/bin/embree.dll" DESTINATION ${CMAKE_BINARY_DIR})
else()
    set(TBB_PATH ${CMAKE_CURRENT_BINARY_DIR}/tbb_cmake_build/tbb_cmake_build_subdir_release)

    execute_process(COMMAND python "${CMAKE_CURRENT_LIST_DIR}/cmake/copy_shared_libs.py"
        # To:
        "${CMAKE_BINARY_DIR}/shared_libs/"
        # From:
        #"${THIRD_PARTY_INSTALL_FOLDER}/GLEW/lib"
        "${THIRD_PARTY_INSTALL_FOLDER}/assimp/lib"
        "${THIRD_PARTY_INSTALL_FOLDER}/embree/lib"
        "${TBB_PATH}")

    if (BUILD_ATLAS)
        execute_process(COMMAND python "${CMAKE_CURRENT_LIST_DIR}/cmake/copy_shared_libs.py"
            # To:
            "${CMAKE_BINARY_DIR}/shared_libs/"
            # From:
            "${THIRD_PARTY_INSTALL_FOLDER}/GLEW/lib")
    endif()

    message("Please execute the following line of code to point your system to the shared libraries:")
    message("export LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/shared_libs:$LD_LIBRARY_PATH")
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
    	set(glew_lib_dir "${THIRD_PARTY_INSTALL_FOLDER}/GLEW/lib")
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