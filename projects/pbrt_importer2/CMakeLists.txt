project("pbrt_importer2")

add_library(pbrt_importer2 STATIC "")
include("${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt")
target_include_directories(pbrt_importer2 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")
target_compile_features(pbrt_importer2 PUBLIC cxx_std_17)

target_link_libraries(pbrt_importer2 PUBLIC
	libPandora
	eastl
	spdlog::spdlog
	fmt::fmt 
	mio::mio)

include("${CMAKE_CURRENT_LIST_DIR}/apps/CMakeLists.txt")
if (PANDORA_BUILD_TESTS)
	include("${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt")
endif()
