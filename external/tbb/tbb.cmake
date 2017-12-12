if (WIN32)
set(TBB_PLATFORM_POSTFIX "win")
elseif (APPLE)
set(TBB_PLATFORM_POSTFIX "mac")
else()# Assume Linux
set(TBB_PLATFORM_POSTFIX "lin")
endif()
set(TBB_PATH "${CMAKE_CURRENT_LIST_DIR}/tbb2018_20171205oss_${TBB_PLATFORM_POSTFIX}/tbb2018_20171205oss/")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${TBB_PATH}")
find_package(TBB)

message("TBB BIN DIR: ${TBB_BINDIR}")
if (WIN32)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
file(COPY "${TBB_PATH}/bin/ia32/vc14/" DESTINATION ${CMAKE_BINARY_DIR})
else ()
file(COPY "${TBB_PATH}/bin/intel64/vc14/" DESTINATION ${CMAKE_BINARY_DIR})
endif ()

elseif (NOT APPLE)# Must be Linux?

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
file(COPY "${TBB_PATH}/bin/ia32/gcc4.7/" DESTINATION ${CMAKE_BINARY_DIR})
else ()
file(COPY "${TBB_PATH}/bin/intel64/gcc4.7/" DESTINATION ${CMAKE_BINARY_DIR})
endif ()

endif()