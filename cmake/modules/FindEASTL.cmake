# Original: https://github.com/mburr/eastl-test/blob/master/cmake_module/FindEASTL.cmake

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	list(APPEND EASTL_DEFINITIONS "-D EASTL_VA_COPY_ENABLED=0")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	list(APPEND EASTL_DEFINITIONS "-D _DEBUG" "-D EA_DEBUG" "-D EASTL_ASSERT_ENABLED")
endif()

find_path(EASTL_INCLUDE_DIR "EASTL/internal/config.h" ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(DEFINED EASTL_SKIP_LIBRARY_FINDING)
  if(EXISTS ${EASTL_INCLUDE_DIR})
    set(EASTL_FOUND True)
  else()
    set(EASTL_FOUND False)
  endif()
else()
  find_library(EASTL_LIBRARY EASTL)
  if(
      (EXISTS ${EASTL_LIBRARY}) AND
      (EXISTS ${EASTL_INCLUDE_DIR})
      )
    set(EASTL_FOUND True)
  else()
    set(EASTL_FOUND False)
  endif()
endif()

if (EASTL_FOUND AND NOT TARGET eastl::eastl)
	add_library(eastl::eastl INTERFACE IMPORTED)
	target_include_directories(eastl::eastl INTERFACE "${EASTL_INCLUDE_DIR}")
	target_link_libraries(eastl::eastl INTERFACE "${EASTL_LIBRARY}")
endif()
