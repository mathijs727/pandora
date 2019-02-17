add_library(stream "")
target_include_directories(stream
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src"
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(stream PUBLIC cxx_std_17)

#target_link_libraries(stream PUBLIC nlohmann_json::nlohmann_json Threads::Threads GSL)
target_link_libraries(stream PUBLIC GSL)

include("${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/include/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt")
