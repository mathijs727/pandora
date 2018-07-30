add_executable(voxel "")
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(voxel PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/)
target_compile_features(voxel PRIVATE cxx_std_17)

if (MSVC)
	# Prefent sscanf warnings in glewinfo.c
	target_compile_definitions(voxel PRIVATE "_CRT_SECURE_NO_WARNINGS")

	# Prevent "undesirable base address for x64 image" warning.
	# Not sure what (in CMake) is causing this linker value to be set anyways
	# https://developercommunity.visualstudio.com/content/problem/160970/upgrading-from-154-to-155-throw-lnk4281-warning.html
	set_target_properties(voxel PROPERTIES COMPILE_FLAGS "/wd4281 /wd4005 /wd4996") 
endif()

target_link_libraries(voxel PRIVATE 
	libPandora
	tinyply
	glm)
