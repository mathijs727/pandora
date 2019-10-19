# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.12)
project("pbrt_importer2")

add_executable(pbrt_importer2 "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(pbrt_importer2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
target_compile_features(pbrt_importer2 PRIVATE cxx_std_17)

target_link_libraries(pbrt_importer2 PRIVATE
	Boost::program_options 
	spdlog::spdlog fmt::fmt)
