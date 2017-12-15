add_executable(atlas "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(atlas PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
target_compile_features(atlas PRIVATE cxx_std_17)

target_compile_definitions(atlas PRIVATE "GLEW_STATIC=1")
target_link_libraries(atlas PRIVATE 
	libPandora
	glfw
	OpenGL::GL
    GLEW::GLEW
	Threads::Threads)
