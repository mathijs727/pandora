# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.11)

project("diskbench")

add_executable(diskbench "main.cpp")
target_link_libraries(diskbench PRIVATE mio)
target_compile_features(diskbench PUBLIC cxx_std_17)

if (LINK_STDCXX_FILESYSTEM)
	target_link_libraries(diskbench PRIVATE stdc++fs)
endif()
if (LINK_CXX_FILESYSTEM)
	target_link_libraries(diskbench PRIVATE c++fs)
endif()

