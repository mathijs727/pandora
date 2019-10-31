project("pbrt_importer2")

add_library(libPbrt_importer2 STATIC "")
include("${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt")
target_include_directories(libPbrt_importer2 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")
target_compile_features(libPbrt_importer2 PUBLIC cxx_std_17)
enable_native_isa(libPbrt_importer2)

if (PANDORA_ISA_AVX512)
	target_compile_definitions(libPbrt_importer2 PUBLIC -DPBRT_ATOF_AVX512=1)
endif()

target_link_libraries(libPbrt_importer2 PUBLIC
	libPandora
	eastl
	spdlog::spdlog
	fmt::fmt
	mio::mio
	TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)

include("${CMAKE_CURRENT_LIST_DIR}/apps/CMakeLists.txt")
if (PANDORA_BUILD_TESTS)
	include("${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt")
endif()
