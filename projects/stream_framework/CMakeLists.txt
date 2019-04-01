add_library(stream
	"src/data_stream.cpp"
	"include/stream/data_stream.h"
	"include/stream/filter_task.h"
	"include/stream/multi_type_scatter_transform_task.h"
	"include/stream/task.h"
	"include/stream/task_pool.h"
	"include/stream/transform_task.h")

target_include_directories(stream PUBLIC "include")
target_compile_features(stream PUBLIC cxx_std_17)
target_link_libraries(stream PUBLIC fmt::fmt spdlog::spdlog mio::mio TBB::tbb)

add_subdirectory("example")
add_subdirectory("tests")