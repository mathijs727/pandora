cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

if (MSVC)
	add_compile_options(/W3 /WX)
else()
	add_compile_options(-W -Wall -Wpedantic)
endif()

# Output to a folder called bin to prevent the following error:
# cannot open output file pandora: Is a directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(libPandora/CMakeLists.txt)
include(pandora/CMakeLists.txt)