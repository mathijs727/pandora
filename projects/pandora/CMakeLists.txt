add_library(libPandora "")
target_compile_features(libPandora PRIVATE cxx_std_17)

if (MSVC)
    target_compile_definitions(libPandora PUBLIC "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS")
	target_compile_definitions(libPandora PUBLIC "_CRT_SECURE_NO_WARNINGS")
	target_compile_definitions(libPandora PUBLIC "-D_SCL_SECURE_NO_WARNINGS")
	target_compile_definitions(libPandora PUBLIC "NOMINMAX")
    target_compile_options(libPandora PRIVATE /W3 /WX)
else()
    target_compile_options(libPandora PRIVATE -W -Wall -Wpedantic -Wno-unused-parameter -Wno-unused-private-field)
endif()
target_compile_definitions(libPandora PRIVATE "ENABLE_STATIC_LIB")# Embree static lib

target_link_libraries(libPandora PUBLIC
    assimp_target
    Boost::boost
    TBB::tbb
    #embree_target
    GSL)

target_include_directories(libPandora PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/)
target_include_directories(libPandora PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)

if (BUILD_TESTS)
    include(${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt)
endif()