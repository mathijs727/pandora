#find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(pandoraTest
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

target_link_libraries(pandoraTest PRIVATE GTest::GTest libPandora)
target_compile_features(pandoraTest PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(pandoraTest PUBLIC "/wd4251" "/wd4275")# Gtest warnings about dll stuff
endif()

enable_testing()
add_test(TARGET pandoraTest)