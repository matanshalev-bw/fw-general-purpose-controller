cmake_minimum_required(VERSION 3.16)
project(gpc_usb_bluelink NONE)

set(TOOL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

find_program(AARCH64_GCC aarch64-linux-gnu-g++)
if(AARCH64_GCC)
  set(AARCH64_AVAILABLE TRUE)
else()
  set(AARCH64_AVAILABLE FALSE)
endif()

add_custom_command(
  OUTPUT ${OUTPUT_DIR}/gpc_usb_bluelink_x86_64
  COMMAND ${CMAKE_COMMAND} -E chdir ${TOOL_DIR} make CC=g++ clean
  COMMAND ${CMAKE_COMMAND} -E chdir ${TOOL_DIR} make CC=g++ all
  COMMAND ${CMAKE_COMMAND} -E copy ${TOOL_DIR}/gpc_usb_bluelink_x86_64 ${OUTPUT_DIR}/gpc_usb_bluelink_x86_64
  DEPENDS ${TOOL_DIR}/makefile
  COMMENT "Building gpc_usb_bluelink_x86_64"
  VERBATIM
)

add_custom_target(gpc_usb_bluelink_x86_64_target DEPENDS ${OUTPUT_DIR}/gpc_usb_bluelink_x86_64)

if(AARCH64_AVAILABLE)
  add_custom_command(
    OUTPUT ${OUTPUT_DIR}/gpc_usb_bluelink_aarch64
    COMMAND ${CMAKE_COMMAND} -E chdir ${TOOL_DIR} make CC=aarch64-linux-gnu-g++ clean
    COMMAND ${CMAKE_COMMAND} -E chdir ${TOOL_DIR} make CC=aarch64-linux-gnu-g++ all
    COMMAND ${CMAKE_COMMAND} -E copy ${TOOL_DIR}/gpc_usb_bluelink_aarch64 ${OUTPUT_DIR}/gpc_usb_bluelink_aarch64
    DEPENDS ${TOOL_DIR}/makefile
    COMMENT "Building gpc_usb_bluelink_aarch64"
    VERBATIM
  )
  add_custom_target(gpc_usb_bluelink_aarch64_target DEPENDS ${OUTPUT_DIR}/gpc_usb_bluelink_aarch64)
  add_dependencies(gpc_usb_bluelink_aarch64_target gpc_usb_bluelink_x86_64_target)
endif()

add_custom_target(gpc_usb_bluelink_all DEPENDS gpc_usb_bluelink_x86_64_target)
if(AARCH64_AVAILABLE)
  add_dependencies(gpc_usb_bluelink_all gpc_usb_bluelink_aarch64_target)
endif()
