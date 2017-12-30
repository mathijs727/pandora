function(find_or_install_package PACKAGE_NAME)
    # Add the location where it should be installed to CMAKE_PREFIX_PATH and [PACKAGENAME]_ROOT
    string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UPPER)
    set(PACKAGE_PATH "${THIRD_PARTY_INSTALL_FOLDER}/${PACKAGE_NAME}/")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${PACKAGE_PATH}")
    set(${PACKAGE_NAME_UPPER}_ROOT ${PACKAGE_PATH})

	if (NOT EXISTS "${PACKAGE_PATH}")
		message("Downloading and compiling dependency: ${PACKAGE_NAME}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME} ${CMAKE_GENERATOR} ${CMAKE_BUILD_TYPE}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()

    if (NOT ${ARGN} STREQUAL "")
        # Optionally pass the version that you want (some find_package implementations require that the user speciifes a version)
        find_package(${PACKAGE_NAME} ${ARGN} REQUIRED)
    else()
        find_package(${PACKAGE_NAME} REQUIRED)
    endif()
    unset(PACKAGE_PATH)
	unset(INSTALL_FOLDER)
    unset(PACKAGE_NAME_UPPER)
endfunction(find_or_install_package)


function(find_or_install_legacy_package PACKAGE_NAME INCLUDE_PATH LIBS_PATH)
    if (NOT ${ARGN} STREQUAL "")
        find_or_install_package(${PACKAGE_NAME} ${ARGN})
        # find_package again because the variables that were set are local to find_or_install_package
        find_package(${PACKAGE_NAME} ${ARGN} REQUIRED)
    else()
        find_or_install_package(${PACKAGE_NAME})
        # find_package again because the variables that were set are local to find_or_install_package
        find_package(${PACKAGE_NAME} REQUIRED)
    endif()

    add_library(${PACKAGE_NAME}_target INTERFACE)
    target_include_directories(${PACKAGE_NAME}_target INTERFACE ${${INCLUDE_PATH}})
    target_link_libraries(${PACKAGE_NAME}_target INTERFACE ${${LIBS_PATH}})
endfunction(find_or_install_legacy_package)


function(install_header_only_package PACKAGE_NAME)
    set(INSTALL_FOLDER "${THIRD_PARTY_INSTALL_FOLDER}/${PACKAGE_NAME}/")
    if (NOT EXISTS "${INSTALL_FOLDER}")
		message("Downloading and installing dependency: ${PACKAGE_NAME}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME} ${CMAKE_GENERATOR} ${CMAKE_BUILD_TYPE}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()
    add_library(${PACKAGE_NAME} INTERFACE)# Create target for the header only
    target_include_directories(${PACKAGE_NAME} INTERFACE "${INSTALL_FOLDER}/include/")
    unset(INSTALL_FOLDER)
endfunction(install_header_only_package)
