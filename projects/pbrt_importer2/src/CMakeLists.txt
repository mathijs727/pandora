target_sources(pbrt_importer2
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/simd_lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/wald_lexer.cpp"
)