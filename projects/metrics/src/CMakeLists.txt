target_sources(metrics
PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/counter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/offline_exporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stats.cpp")
