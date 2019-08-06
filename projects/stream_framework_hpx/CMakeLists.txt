add_library(stream
	"src/task_executor.cpp")

target_include_directories(stream PUBLIC "include")
target_compile_features(stream PUBLIC cxx_std_17)
target_link_libraries(stream PUBLIC
	fmt::fmt
	spdlog::spdlog
	mio::mio
	hpx hpx_init hpx_preprocessor iostreams_component
	TBB::tbb TBB::tbbmalloc
	Boost::boost)

add_subdirectory("example")

if (STREAM_BUILD_TESTS)
	add_subdirectory("tests")
endif()