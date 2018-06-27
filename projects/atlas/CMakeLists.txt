add_executable(atlas "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(atlas PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
target_compile_features(atlas PRIVATE cxx_std_17)

if (MSVC)
	# Prefent sscanf warnings in glewinfo.c
	target_compile_definitions(atlas PRIVATE "_CRT_SECURE_NO_WARNINGS")

	# Prevent "undesirable base address for x64 image" warning.
	# Not sure what (in CMake) is causing this linker value to be set anyways
	# https://developercommunity.visualstudio.com/content/problem/160970/upgrading-from-154-to-155-throw-lnk4281-warning.html
	set_target_properties(atlas PROPERTIES COMPILE_FLAGS "/wd4281 /wd4005") 
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/atlas_output.fs" "${CMAKE_CURRENT_BINARY_DIR}/atlas_output.fs" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/atlas_output.vs" "${CMAKE_CURRENT_BINARY_DIR}/atlas_output.vs" COPYONLY)

target_link_libraries(atlas PRIVATE 
	libPandora
	glfw
	OpenGL::GL
    GLEW::GLEW)
