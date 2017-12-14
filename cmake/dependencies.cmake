function(find_or_install_package PACKAGE_NAME)
    # Add the location where it should be installed to CMAKE_PREFIX_PATH and [PACKAGENAME]_ROOT
    string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UPPER)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}")
    set(${${PACKAGE_NAME_UPPER}_ROOT}} "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}")

    find_package(${PACKAGE_NAME} QUIET)

    if (NOT ${${PACKAGE_NAME}_FOUND})
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
        find_package(${PACKAGE_NAME} REQUIRED)
    endif()
endfunction(find_or_install_package)

function(install_header_only_package PACKAGE_NAME)
    set(install_folder "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}/include/")
    if (NOT EXISTS "${install_folder}")
        execute_process(
            COMMAND python external_dependencies.py ${PACKAGE_NAME}
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/")
    endif()
    add_library(${PACKAGE_NAME} INTERFACE)# Create target for the header only
    target_include_directories(${PACKAGE_NAME} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/third_party/install/${PACKAGE_NAME}/include/")
    unset(install_folder)
endfunction(install_header_only_package)
