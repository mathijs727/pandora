add_executable(pandoraTest
	${CMAKE_CURRENT_LIST_DIR}/my_first_test.cpp)
target_compile_features(libPandora PRIVATE cxx_std_17)

add_subdirectory("${PROJECT_SOURCE_DIR}/external/googletest/")
target_link_libraries(pandoraTest PRIVATE gtest)

enable_testing()
add_test(
	NAME pandoraTests
	COMMAND pandoraTest)