cmake_minimum_required(VERSION 3.25)

project(programmer-tools-gpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_FLAGS "-Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(BLUELINK_MSG ${REPO_ROOT}/3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include)
set(BLUELINK_VER ${REPO_ROOT}/3rd_party/bluelink_sdk/bluelink_version)
set(BLUELINK_SER ${REPO_ROOT}/3rd_party/bluelink_sdk/bluelink_serializer/bluelink_serializer_include)
set(LLC_COMMON ${REPO_ROOT}/../fw-llc/scripts/common)

include_directories(
    ${REPO_ROOT}
    ${BLUELINK_MSG}
    ${BLUELINK_MSG}/PayloadStructs
    ${BLUELINK_VER}
    ${BLUELINK_SER}
    ${BLUELINK_SER}/Communicator
    ${BLUELINK_SER}/Communicator/BluelinkCommHandler
    ${BLUELINK_SER}/PacketStruct
    ${BLUELINK_SER}/Serialization/CRC32
    ${LLC_COMMON}
    ${REPO_ROOT}/interfaces/interfaces_g474
    ${CMAKE_CURRENT_SOURCE_DIR}/g474
)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(PROGRAMMER_SOURCES
    g474/programmer.cpp
    g474/can_transport.cpp
    g474/usb_transport.cpp
    g474/concrete_bluelink_callbacks.cpp
    ${BLUELINK_SER}/Communicator/AckVector/ack_vector.cpp
    ${BLUELINK_SER}/Communicator/bluelink_callbacks.cpp
    ${BLUELINK_SER}/Communicator/BluelinkCommHandler/bluelink_communication_handler.cpp
    ${BLUELINK_SER}/Serialization/CRC32/checksum_32.cpp
    ${BLUELINK_SER}/Serialization/Deserializer/deserializer.cpp
    ${BLUELINK_SER}/Serialization/Serializer/serializer.cpp
    ${LLC_COMMON}/serial.cpp
    ${LLC_COMMON}/unix.cpp
)

add_executable(prog-gpc-g4 ${PROGRAMMER_SOURCES})

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
