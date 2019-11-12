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

target_link_libraries(libPbf_importer PUBLIC
	libPandora
	spdlog::spdlog
	fmt::fmt
	mio::mio
	mio_cache_control)
