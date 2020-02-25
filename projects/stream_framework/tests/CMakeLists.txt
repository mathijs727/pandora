add_executable(stream_test
	"lru_cache.cpp"
	"lru_cache_ts.cpp"
	"task_graph.cpp"
	"file_serializer.cpp"
	"tbb_queue.cpp"
	"moodycamel_queue.cpp"
	"main.cpp")

target_link_libraries(stream_test PRIVATE GTest::GTest stream TBB::tbb)
target_compile_features(stream_test PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(stream_test PUBLIC "/wd4251" "/wd4275") # Gtest warnings about dll stuff
endif()

gtest_add_tests(TARGET stream_test)