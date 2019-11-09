project("pbf_importer")

add_library(libPbf_importer STATIC "")
include("${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt")
target_include_directories(libPbf_importer
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src/"
	PUBLIC 
		"${CMAKE_CURRENT_LIST_DIR}/include/")
target_compile_features(libPbf_importer PUBLIC cxx_std_17)
enable_native_isa(libPbf_importer)

#if (PANDORA_ISA_AVX512)
#	target_compile_definitions(libPbrt_importer2 PUBLIC -DPBRT_ATOF_AVX512=1)
#endif()

target_link_libraries(libPbf_importer PUBLIC
	libPandora
	spdlog::spdlog
	fmt::fmt
	mio::mio)

#include("${CMAKE_CURRENT_LIST_DIR}/apps/CMakeLists.txt")
#if (PANDORA_BUILD_TESTS)
#	include("${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt")
#endif()
