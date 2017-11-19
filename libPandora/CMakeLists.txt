add_library(libPandora "")

target_include_directories(libPandora PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/)
target_include_directories(libPandora PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)

include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)

set_target_properties(libPandora PROPERTIES
    CXX_STANDARD 17# Require C++ 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)


include(${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt)