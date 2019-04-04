add_executable(stream_test
    data_stream.cpp)

target_link_libraries(stream_test PRIVATE GTest::GTest stream)
target_compile_features(stream_test PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(stream_test PUBLIC "/wd4251" "/wd4275") # Gtest warnings about dll stuff
endif()

#add_test(AllTestsInStream stream_test)
gtest_add_tests(TARGET stream_test)