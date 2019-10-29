target_sources(libPbrt_importer2
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lexer/lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lexer/simd_lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lexer/wald_lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/util/crack_atof.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/util/crack_atof_sse.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/util/crack_atof_avx2.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/parser/parser.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/pbrt_importer.cpp")

if (${PANDORA_ISA_AVX512})
	target_sources(libPbrt_importer2 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/util/crack_atof_avx512.cpp")
endif()