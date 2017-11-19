cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

if (MSVC)
	add_compile_options(/W3 /WX)
else()
	add_compile_options(-W -Wall -Wpedantic)
endif()

include(libPandora/CMakeLists.txt)
include(pandora/CMakeLists.txt)