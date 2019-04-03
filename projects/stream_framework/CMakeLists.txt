add_library(stream
	"src/source_task.cpp"
	"src/task_pool.cpp"

	"include/stream/data_stream.h"
	#"include/stream/filter_task.h"
	#"include/stream/multi_type_scatter_transform_task.h"
	"include/stream/resource_cache.h"
	"include/stream/source_task.h"
	"include/stream/task.h"
	"include/stream/task_pool.h"
	"include/stream/traits.h"
	"include/stream/transform_task.h")

target_include_directories(stream PUBLIC "include")
target_compile_features(stream PUBLIC cxx_std_17)
target_link_libraries(stream PUBLIC fmt::fmt spdlog::spdlog mio::mio TBB::tbb TBB::tbbmalloc Boost::boost)

add_subdirectory("example")

if (STREAM_BUILD_TESTS)
	add_subdirectory("tests")
endif()