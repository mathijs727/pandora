target_sources(metrics
PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/counter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/gauge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/histogram.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/offline_exporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stats.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stopwatch.cpp")
