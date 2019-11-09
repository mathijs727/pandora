target_sources(libPbf_importer
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lexer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/pbf_importer.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/parser.cpp")
