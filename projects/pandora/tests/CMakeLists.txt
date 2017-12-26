#find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(pandoraTest
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_allocator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_transform.cpp
	${CMAKE_CURRENT_LIST_DIR}/test_vec3.cpp)

target_link_libraries(pandoraTest PRIVATE GTest::GTest libPandora)
target_compile_features(pandoraTest PRIVATE cxx_std_17)

enable_testing()
add_test(TARGET pandoraTest)