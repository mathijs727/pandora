add_executable(pandora "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)

target_include_directories(pandora PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)

set_target_properties(pandora PROPERTIES
    CXX_STANDARD 17# Require C++ 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_link_libraries(pandora PRIVATE libPandora)
