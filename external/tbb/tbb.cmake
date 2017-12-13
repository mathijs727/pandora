# If on Windows, use precompiled binaries because TBB uses GNU Make as build system (welcome to 2017/2018 Intel...)
if (WIN32)

set(TBB_PATH "${CMAKE_CURRENT_LIST_DIR}/tbb2018_20171205oss_win/tbb2018_20171205oss/")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${TBB_PATH}")
find_package(TBB REQUIRED tbb)

# Copy the DLL files to the binary folder
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
file(COPY "${TBB_PATH}/bin/ia32/vc14/" DESTINATION ${CMAKE_BINARY_DIR})
else ()
file(COPY "${TBB_PATH}/bin/intel64/vc14/" DESTINATION ${CMAKE_BINARY_DIR})
endif ()

else ()# Must be Linux?

include("${CMAKE_CURRENT_LIST_DIR}/tbb/cmake/TBBBuild.cmake")
tbb_build(TBB_ROOT "${CMAKE_CURRENT_LIST_DIR}/tbb/" CONFIG_DIR TBB_DIR)
find_package(TBB REQUIRED tbb)
#if (CMAKE_SIZEOF_VOID_P EQUAL 4)
#file(COPY "${TBB_PATH}/bin/ia32/gcc4.7/" DESTINATION ${CMAKE_BINARY_DIR})
#else ()
#file(COPY "${TBB_PATH}/bin/intel64/gcc4.7/" DESTINATION ${CMAKE_BINARY_DIR})
#endif ()

endif()