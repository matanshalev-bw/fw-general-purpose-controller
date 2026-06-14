cmake_minimum_required(VERSION 3.25)

set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

project(fw-gpc LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(DEFINED BUILD_COMPONENT)
    message(STATUS "Building specific component: ${BUILD_COMPONENT}")
    if(BUILD_COMPONENT STREQUAL "fw-config-g4")
        add_subdirectory(config_projects/config_g474)
    elseif(BUILD_COMPONENT STREQUAL "fw-bootloader-g4")
        add_subdirectory(bootloader_projects/bootloader_g474)
    elseif(BUILD_COMPONENT STREQUAL "application-g474")
        add_subdirectory(application_projects/application_g474)
    else()
        message(FATAL_ERROR
            "Invalid BUILD_COMPONENT: ${BUILD_COMPONENT}. "
            "Must be one of: fw-config-g4, fw-bootloader-g4, application-g474")
    endif()
else()
    message(FATAL_ERROR "BUILD_COMPONENT must be set (e.g. fw-config-g4)")
endif()
