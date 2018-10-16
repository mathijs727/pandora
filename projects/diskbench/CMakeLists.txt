# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.11)

project("diskbench")

add_executable(diskbench "main.cpp")
target_link_libraries(diskbench PRIVATE mio)
target_compile_features(diskbench PUBLIC cxx_std_17)

