# assimp config does not follow the CMake naming conventions, does not export a target
#  and (worst of all): does not return the full path to the library files
function(find_or_install_assimp)
	set(PACKAGE_NAME "assimp")
    set(INSTALL_FOLDER "${THIRD_PARTY_INSTALL_FOLDER}/${PACKAGE_NAME}/")
    if (NOT EXISTS "${INSTALL_FOLDER}")
        message("Downloading and compiling dependency: ${PACKAGE_NAME}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME} ${CMAKE_GENERATOR} ${CMAKE_BUILD_TYPE}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()

	set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${INSTALL_FOLDER}")
	set(ASSIMP_DIR "${INSTALL_FOLDER}")
	find_package(assimp REQUIRED)

    find_library(LIBRARY
		NAMES "${ASSIMP_LIBRARIES}"
		PATHS "${INSTALL_FOLDER}/lib")
	
	# On windows copy the DLL files to the destination folder
	if (WIN32)
		file(COPY "${INSTALL_FOLDER}/bin/"
			DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
	endif()

    add_library(${PACKAGE_NAME}_target INTERFACE)
    target_include_directories(${PACKAGE_NAME}_target INTERFACE ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(${PACKAGE_NAME}_target INTERFACE ${LIBRARY})
endfunction(find_or_install_assimp)