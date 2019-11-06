cmake_minimum_required(VERSION 3.11)
project(Optick LANGUAGES CXX)

file(GLOB OPTICK_SRC "src/*.*")

# Enabled
option(OPTICK_ENABLED "Enable profiling with Optick" OFF)

add_library(OptickCore STATIC ${OPTICK_SRC})
target_include_directories(OptickCore
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/include/optick/"
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include/")
target_compile_definitions(OptickCore PRIVATE OPTICK_ENABLE_GPU=0)
target_link_libraries(OptickCore PRIVATE fmt::fmt)

if (NOT OPTICK_ENABLED)
	message(STATUS "Optick is disabled")
	target_compile_definitions(OptickCore PUBLIC USE_OPTICK=0)
endif()
