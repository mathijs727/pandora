# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.12)

project("pandora_python")

add_executable(string_to_float_benchmark "benchmark.cpp" "src/crack_atof.cpp" "src/crack_atof_sse.cpp" "src/crack_atof_avx2.cpp" "src/crack_atof_avx512.cpp")
#target_link_libraries(string_to_float_benchmark PRIVATE TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)
target_compile_features(string_to_float_benchmark PRIVATE cxx_std_17)
