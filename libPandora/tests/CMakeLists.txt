find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(pandoraTest
	${CMAKE_CURRENT_LIST_DIR}/my_first_test.cpp)

target_link_libraries(pandoraTest PRIVATE gtest Threads::Threads)
target_compile_features(pandoraTest PRIVATE cxx_std_17)

enable_testing()
gtest_add_tests(TARGET pandoraTest)