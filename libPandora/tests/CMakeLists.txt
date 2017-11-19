find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(pandoraTest
	${CMAKE_CURRENT_LIST_DIR}/my_first_test.cpp)
target_include_directories(pandoraTest PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(pandoraTest PRIVATE
	${GTEST_BOTH_LIBRARIES}
	Threads::Threads)

enable_testing()
add_test(
	NAME pandoraTests
	COMMAND pandoraTest)