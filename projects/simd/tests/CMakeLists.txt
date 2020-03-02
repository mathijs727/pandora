add_executable(simd_test
    ${CMAKE_CURRENT_LIST_DIR}/src/test_simd4.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/test_simd8.cpp)

target_link_libraries(simd_test PRIVATE GTest::gtest GTest::gtest_main simd)
target_compile_features(simd_test PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(simd_test PUBLIC "/wd4251" "/wd4275") # Gtest warnings about dll stuff
endif()

#add_test(AllTestsInSIMD simd_test)
add_test(AllSIMDTests simd_test)