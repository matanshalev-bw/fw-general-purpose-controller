cmake_minimum_required(VERSION 3.25)

project(programmer-tools-gpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_FLAGS "-Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rd_party/bluelink_sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rd_party/bluelink_sdk/bluelink_version
    ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/interfaces_g474
)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

add_executable(prog-gpc-g4
    g474/programmer.cpp
)

set_target_properties(prog-gpc-g4 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(prog-gpc-g4 pthread)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set_target_properties(prog-gpc-g4 PROPERTIES OUTPUT_NAME "prog-gpc-g4_aarch64")
        message(STATUS "Building for ARM64 (aarch64)")
    elseif(CMAKE_CXX_COMPILER MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set_target_properties(prog-gpc-g4 PROPERTIES OUTPUT_NAME "prog-gpc-g4_x86_64")
        message(STATUS "Building for x86_64")
    else()
        set_target_properties(prog-gpc-g4 PROPERTIES OUTPUT_NAME "prog-gpc-g4")
        message(STATUS "Building for default architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

include(GNUInstallDirs)

install(TARGETS prog-gpc-g4
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT tools
)

install(FILES
    can_setup.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    COMPONENT tools
)

install(FILES
    README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/programmer-tools
    COMPONENT documentation
)

target_compile_definitions(prog-gpc-g4 PRIVATE TARGET_MCU_G4)
target_compile_features(prog-gpc-g4 PRIVATE cxx_std_14)
