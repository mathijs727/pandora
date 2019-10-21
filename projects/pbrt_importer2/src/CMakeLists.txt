target_sources(pbrt_importer2
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lexer/lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lexer/simd_lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lexer/wald_lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/parser/parser.cpp")