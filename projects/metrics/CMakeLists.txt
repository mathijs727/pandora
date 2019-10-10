add_library(metrics "")
target_include_directories(metrics
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src"
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(metrics PUBLIC cxx_std_17)

target_link_libraries(metrics PUBLIC nlohmann_json::nlohmann_json Threads::Threads)

include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/include/CMakeLists.txt)
#include(${CMAKE_CURRENT_LIST_DIR}/example/CMakeLists.txt)
