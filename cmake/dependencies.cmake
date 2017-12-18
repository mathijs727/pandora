function(find_or_install_package PACKAGE_NAME)
    # Add the location where it should be installed to CMAKE_PREFIX_PATH and [PACKAGENAME]_ROOT
    string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UPPER)
    set(PACKAGE_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${PACKAGE_PATH}")
    set(${PACKAGE_NAME_UPPER}_ROOT ${PACKAGE_PATH})

    set(INSTALL_FOLDER "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}/")
	if (NOT EXISTS "${INSTALL_FOLDER}")
		message("Downloading and compiling dependency: ${PACKAGE_NAME}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME} ${CMAKE_GENERATOR}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()
    find_package(${PACKAGE_NAME} REQUIRED)
    unset(PACKAGE_PATH)
	unset(INSTALL_FOLDER)
    unset(PACKAGE_NAME_UPPER)
endfunction(find_or_install_package)

function(install_header_only_package PACKAGE_NAME)
    set(INSTALL_FOLDER "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}/")
    if (NOT EXISTS "${INSTALL_FOLDER}")
		message("Downloading and installing dependency: ${PACKAGE_NAME}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME} ${CMAKE_GENERATOR}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()
    add_library(${PACKAGE_NAME} INTERFACE)# Create target for the header only
    target_include_directories(${PACKAGE_NAME} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}/include/")
    unset(INSTALL_FOLDER)
endfunction(install_header_only_package)


# Manually find include and lib folder for packages that do not provide any CMake code.
# Or for packages where the CMake config code is so bad that it's unusable.
# Basically: assimp config does not follow the CMake naming conventions, does not export a target
#  and (worst of all): does not return the full path to the library
function(find_or_install_legacy_package PACKAGE_NAME)
    set(INSTALL_FOLDER "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}/")
    if (NOT EXISTS "${INSTALL_FOLDER}")
        message("Downloading and compiling dependency: ${PACKAGE_NAME}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME} ${CMAKE_GENERATOR}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()

    #find_path(${PACKAGE_NAME}_INCLUDE_DIR NAMES ${PACKAGE_NAME}/some_header_file.h PATHS "${INSTALL_FOLDER}/include")
    set(INCLUDE_DIR ${INSTALL_FOLDER}/include)
    find_library(LIBRARY NAMES ${PACKAGE_NAME} PATHS "${INSTALL_FOLDER}/lib")

    add_library(${PACKAGE_NAME}_target "")
    target_include_directories(${PACKAGE_NAME}_target PUBLIC ${INCLUDE_DIR})
    set_target_properties(${PACKAGE_NAME}_target PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${PACKAGE_NAME}_target PUBLIC ${LIBRARY})
endfunction(find_or_install_legacy_package)