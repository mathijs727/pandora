function(install_and_find_tbb)
	set(TBB_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/downloads/tbb")
	
	if (NOT EXISTS ${tbb_path})
		execute_process(
            COMMAND python external_dependencies.py tbb unusedargument ${CMAKE_BUILD_TYPE}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
	endif()

	if (WIN32)# If on Windows, use precompiled binaries because TBB build requires GNU Make
		set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${TBB_PATH}")
		find_package(TBB REQUIRED tbb)

		# Copy the DLL files to the binary folder
		if (CMAKE_SIZEOF_VOID_P EQUAL 4)
			file(COPY "${TBB_PATH}/bin/ia32/vc14/" DESTINATION ${CMAKE_BINARY_DIR})
		else ()
			file(COPY "${TBB_PATH}/bin/intel64/vc14/" DESTINATION ${CMAKE_BINARY_DIR})
		endif ()
	else ()# If not Windows: build from source
		include("${TBB_PATH}/cmake/TBBBuild.cmake")
		tbb_build(TBB_ROOT "${TBB_PATH}" CONFIG_DIR TBB_DIR MAKE_ARGS)
		find_package(TBB REQUIRED tbb)
	endif ()
	unset(TBB_PATH)
endfunction(install_and_find_tbb)
