add_executable(prometheus_test "")
target_include_directories(prometheus_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_compile_features(prometheus_test PRIVATE cxx_std_17)

include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)

target_link_libraries(prometheus_test
	PRIVATE
		prometheus-cpp::core
		prometheus-cpp::pull
		prometheus-cpp::push
		wsock32 ws2_32)
