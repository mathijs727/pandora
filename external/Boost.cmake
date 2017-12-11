find_package(Boost)

# Create targets for boost using modern CMake (so that header files are included as well)
add_library(boost_multi_array INTERFACE)
target_include_directories(boost_multi_array SYSTEM INTERFACE "${Boost_INCLUDE_DIRS}")


add_library(boost_fiber "")
target_link_libraries(boost_fiber PUBLIC Boost::fiber)
target_include_directories(boost_fiber SYSTEM PUBLIC "${Boost_INCLUDE_DIRS}")