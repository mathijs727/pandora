add_library(stream
	src/task_graph.cpp)

target_include_directories(stream PUBLIC "include")
target_compile_features(stream PUBLIC cxx_std_17)
target_link_libraries(stream PUBLIC
	eastl
	fmt::fmt
	spdlog::spdlog
	TBB::tbb TBB::tbbmalloc Threads::Threads)

add_subdirectory("example")

if (STREAM_BUILD_TESTS)
	add_subdirectory("tests")
endif()