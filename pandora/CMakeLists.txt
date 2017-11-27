add_executable(pandora "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(pandora PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
target_compile_features(pandora PRIVATE cxx_std_14)

target_link_libraries(pandora PRIVATE 
	libPandora
	glfw
    customGlew
	Threads::Threads)
