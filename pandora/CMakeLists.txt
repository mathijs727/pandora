add_executable(pandora "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(pandora PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
target_compile_features(pandora PRIVATE cxx_std_17)

find_package(Threads REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(pandora PRIVATE 
	libPandora
	glfw
	${GLEW_LIBRARIES}
	Threads::Threads)
