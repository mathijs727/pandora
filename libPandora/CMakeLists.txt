add_library(libPandora "")
target_compile_features(libPandora PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options(libPandora PRIVATE /W3 /WX)
else()
    target_compile_options(libPandora PRIVATE -W -Wall -Wpedantic)
endif()

target_include_directories(libPandora PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/)
target_include_directories(libPandora PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)


include(${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt)