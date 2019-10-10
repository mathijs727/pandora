find_package(embree 3 REQUIRED)

add_library(embree3::embree3 INTERFACE IMPORTED)
target_include_directories(embree3::embree3 INTERFACE "${EMBREE_INCLUDE_DIRS}")
target_link_libraries(embree3::embree3 INTERFACE "${EMBREE_LIBRARY}")
