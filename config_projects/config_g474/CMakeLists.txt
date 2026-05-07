cmake_minimum_required(VERSION 3.25)

# Set the project name and specify languages
project(fw-config LANGUAGES C CXX ASM)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Define the target MCU (STM32F072 for config firmware)
set(MCU_FAMILY STM32F0xx)
set(MCU_MODEL STM32F072xB)
set(MCU_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F072C8TX_FLASH.ld")

# Project-specific definitions
add_definitions(-DUSE_HAL_DRIVER -DSTM32F072xB)

# Configuration Type Selection
# This can be overridden by CMake presets or command line: -DCONFIG_TYPE=NH_CLOSE_CABIN
if(NOT DEFINED CONFIG_TYPE)
    set(CONFIG_TYPE "4")
endif()

# Validate config type
if(NOT CONFIG_TYPE MATCHES "^(4|5|6|8)$")
    message(FATAL_ERROR "Invalid CONFIG_TYPE: ${CONFIG_TYPE}. Must be 4 (LAB), 5 (NH_OPEN_CABIN), 6 (NH_CLOSE_CABIN), or 8 (TEST)")
endif()

# Add configuration as compiler definitions
add_definitions(-DCONFIG_TYPE=${CONFIG_TYPE})

message(STATUS "Configuration Type: ${CONFIG_TYPE}")

# Architecture flags (for both compilation and linking)
# Exact IDE compilation flags with specs (matching order exactly)
set(CMAKE_C_FLAGS "-mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP --specs=nano.specs -mfloat-abi=soft -mthumb")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m0 -std=gnu++14 -g3 -DDEBUG -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP --specs=nano.specs -mfloat-abi=soft -mthumb")

set(CMAKE_ASM_FLAGS "-mcpu=cortex-m0 -g3 -DDEBUG --specs=nano.specs -mfloat-abi=soft -mthumb")

# Empty debug/release configs since flags are already in base flags
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_ASM_FLAGS_DEBUG "")

set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_ASM_FLAGS_RELEASE "")

# Override CMake's automatic flag passing to match IDE exactly
set(CMAKE_C_LINK_FLAGS "")
set(CMAKE_CXX_LINK_FLAGS "")

# Linker flags (nosys.specs only - nano.specs comes from compilation flags automatically)
set(CMAKE_EXE_LINKER_FLAGS 
    "-T${MCU_LINKER_SCRIPT} --specs=nosys.specs -Wl,-Map=${PROJECT_NAME}.map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group"
)

# Include directories
include_directories(
    Core/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F0xx/Include
    Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/configs/ConfigStructs
    ${CMAKE_SOURCE_DIR}/configs/ConfigStructs/SequencesConfigs
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/f072-Configs
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/f072-Configs/Lab
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/f072-Configs/NHOpenCabin
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/f072-Configs/NHCloseCabin
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/f072-Configs/Test
    ${CMAKE_SOURCE_DIR}/interface/f072/InterfaceDefines
    ${CMAKE_SOURCE_DIR}
)

# HAL Driver sources
file(GLOB HAL_SOURCES
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_cortex.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_dma.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_exti.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash_ex.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_gpio.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c_ex.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr_ex.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc_ex.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_spi.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_spi_ex.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_tim.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_tim_ex.c"
)

# Core application sources
set(CORE_SOURCES
    Core/Src/main.cpp
    Core/Src/stm32f0xx_hal_msp.c
    Core/Src/stm32f0xx_it.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    Core/Src/system_stm32f0xx.c
)

# Startup file
set(STARTUP_ASM Core/Startup/startup_stm32f072c8tx.s)

# Create executable
add_executable(${PROJECT_NAME}.elf
    ${HAL_SOURCES}
    ${CORE_SOURCES}
    ${STARTUP_ASM}
)

# Post-build commands to generate hex and bin files
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    # Generate config-only hex file as the main hex file (matches reference files)
    COMMAND ${CMAKE_OBJCOPY} -O ihex --only-section=.config $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    # Generate config-only bin file as the main bin file (matches reference files)
    COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=.config $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building ${PROJECT_NAME}.hex (config-only) and ${PROJECT_NAME}.bin (config-only)"
)

# Generate additional outputs
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -h -S $<TARGET_FILE:${PROJECT_NAME}.elf> > ${PROJECT_NAME}.list
    COMMENT "Building ${PROJECT_NAME}.list"
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}.elf
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT firmware
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.list
    # ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT firmware
)

