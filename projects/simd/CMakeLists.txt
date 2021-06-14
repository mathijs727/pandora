add_library(simd INTERFACE)
#include("${CMAKE_CURRENT_LIST_DIR}/include/CMakeLists.txt")
target_include_directories(simd INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(simd INTERFACE cxx_std_20)
#target_link_libraries(simd INTERFACE GSL)

enable_native_isa(simd)

option(SIMD_BUILD_TESTS OFF)

if(SIMD_BUILD_TESTS)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()
# We require the Guideline Support Library but Microsofts implementation does not export a target (with vcpkg)
#target_link_libraries(simd INTERFACE GSL)