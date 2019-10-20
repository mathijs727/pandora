add_executable(pbrt_importer2_tests
	"${CMAKE_CURRENT_LIST_DIR}/main.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/test_lexer.cpp")

target_link_libraries(pbrt_importer2_tests PRIVATE GTest::GTest pbrt_importer2)
target_compile_features(pbrt_importer2_tests PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(pbrt_importer2_tests PUBLIC "/wd4251" "/wd4275") # Gtest warnings about dll stuff
endif()

add_custom_command(TARGET pbrt_importer2_tests POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${CMAKE_CURRENT_LIST_DIR}/test_files/" "$<TARGET_FILE_DIR:pbrt_importer2_tests>/test_files/")

gtest_add_tests(TARGET pbrt_importer2_tests)