add_executable(pbrt_importer2_lexer_bench
	"${CMAKE_CURRENT_LIST_DIR}/lexer_bench.cpp")
target_link_libraries(pbrt_importer2_lexer_bench PRIVATE
	libPbrt_importer2
	Boost::program_options
	mio::mio)

add_executable(pbrt_importer2_parser_bench
	"${CMAKE_CURRENT_LIST_DIR}/parser_bench.cpp")
target_link_libraries(pbrt_importer2_parser_bench PRIVATE
	libPbrt_importer2
	Boost::program_options)

add_executable(string_to_float_bench
	"${CMAKE_CURRENT_LIST_DIR}/string_to_float_bench.cpp")
target_link_libraries(string_to_float_bench PRIVATE
	libPbrt_importer2)
