add_executable(pbrt_importer2_lexer_bench
	"${CMAKE_CURRENT_LIST_DIR}/lexer_bench.cpp")
target_link_libraries(pbrt_importer2_lexer_bench PRIVATE
	pbrt_importer2
	Boost::program_options
	mio::mio mio::mio_base)

