add_executable(metrics_example "")
target_include_directories(metrics_example
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src")
target_compile_features(metrics_example PUBLIC cxx_std_20)
target_link_libraries(metrics_example PRIVATE metrics)
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
