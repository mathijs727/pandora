add_executable(stream_tests
    "${CMAKE_CURRENT_LIST_DIR}/src/test_stream.cpp")

target_link_libraries(stream_tests PRIVATE GTest::GTest GTest::Main stream)
target_compile_features(stream_tests PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(stream_tests PUBLIC "/wd4251" "/wd4275") # Gtest warnings about dll stuff
endif()

enable_testing()
add_test(AllStreamTests stream_tests)
#gtest_add_tests(TARGET stream_tests)