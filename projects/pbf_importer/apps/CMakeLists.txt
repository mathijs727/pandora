add_executable(pbf_importer_parser_bench
	"${CMAKE_CURRENT_LIST_DIR}/parser_bench.cpp")
target_link_libraries(pbf_importer_parser_bench PRIVATE
	libPbf_importer
	Boost::program_options
	mio::mio)
