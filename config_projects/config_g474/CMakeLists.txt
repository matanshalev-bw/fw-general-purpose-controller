cmake_minimum_required(VERSION 3.25)

# Set the project name and specify languages
project(fw-config-g4 LANGUAGES C CXX ASM)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Define the target MCU
set(MCU_FAMILY STM32G4xx)
set(MCU_MODEL STM32G474xx)
set(MCU_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32G474CETX_FLASH.ld")

# Project-specific definitions
add_definitions(-DUSE_HAL_DRIVER -DSTM32G474xx)

# Configuration Type Selection
# This can be overridden by CMake presets or command line: -DCONFIG_TYPE=7
if(NOT DEFINED CONFIG_TYPE)
    set(CONFIG_TYPE "1")
endif()

# Validate config type
if(NOT CONFIG_TYPE MATCHES "^(1|2|3|7)$")
    message(FATAL_ERROR "Invalid CONFIG_TYPE: ${CONFIG_TYPE}. Must be 1 (LAB), 2 (NH_OPEN_CABIN), 3 (NH_CLOSE_CABIN), or 7 (TEST)")
endif()

# Add configuration as compiler definitions
add_definitions(-DCONFIG_TYPE=${CONFIG_TYPE})

message(STATUS "Configuration Type: ${CONFIG_TYPE}")

# Mixed optimization approach matching IDE: HAL drivers (-O0), Interface files (-Os)
set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m4 -std=gnu++20 -g3 -DDEBUG -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")

set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -g3 -DDEBUG --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")

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

# Linker flags exactly matching IDE (specs come from compiler flags)
set(CMAKE_EXE_LINKER_FLAGS 
    "-T${MCU_LINKER_SCRIPT} -Wl,-Map=${PROJECT_NAME}.map -Wl,--gc-sections -static -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group"
)

# Include directories in exact IDE order (CRITICAL for correct header resolution)
if(DEFINED GPC_EXPORT_CONFIG_DIR)
    include_directories(BEFORE ${GPC_EXPORT_CONFIG_DIR})
    message(STATUS "GPC export config dir: ${GPC_EXPORT_CONFIG_DIR}")
endif()

include_directories(
    Core/Inc
    Drivers/STM32G4xx_HAL_Driver/Inc
    Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32G4xx/Include
    Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/configs/ConfigStructs
    ${CMAKE_SOURCE_DIR}/configs/ConfigStructs/SequencesConfigs
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/g474-Configs
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/g474-Configs/Lab
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/g474-Configs/NHOpenCabin
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/g474-Configs/NHCloseCabin
    ${CMAKE_SOURCE_DIR}/configs/ConfigsTypes/g474-Configs/Test
    ${CMAKE_SOURCE_DIR}/interface/g474/InterfaceDefines
    ${CMAKE_SOURCE_DIR}/interfaces/interfaces_g474/MemoryMap
    ${CMAKE_SOURCE_DIR}/3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include
    ${CMAKE_SOURCE_DIR}/3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include/PayloadStructs
    ${CMAKE_SOURCE_DIR}
)

# HAL Driver sources matching IDE build exactly
set(HAL_SOURCES
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ramfunc.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c"
    "Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c"
)

# Core application sources
set(CORE_SOURCES
    Core/Src/main.cpp
    Core/Src/stm32g4xx_hal_msp.c
    Core/Src/stm32g4xx_it.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    Core/Src/system_stm32g4xx.c
)

# Startup file
set(STARTUP_ASM Core/Startup/startup_stm32g474cetx.s)

# Create executable
add_executable(${PROJECT_NAME}.elf
    ${HAL_SOURCES}
    ${CORE_SOURCES}
    ${STARTUP_ASM}
)

# Apply no optimization (-O0) to core C++ files to match IDE build exactly
set_source_files_properties(
    Core/Src/main.cpp
    PROPERTIES COMPILE_FLAGS "-O0"
)

# Post-build commands to generate hex and bin files
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    # Generate config-only hex file as the main hex file (matches reference files)
    COMMAND ${CMAKE_OBJCOPY} -O ihex --only-section=.config $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    # Generate config-only bin file as the main bin file (matches reference files)
    COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=.config $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    # CubeIDE artifact name for compatibility with config_g474.bin consumers
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_NAME}.bin config_g474.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building ${PROJECT_NAME}.hex (config-only), ${PROJECT_NAME}.bin, and config_g474.bin"
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
    ${CMAKE_CURRENT_BINARY_DIR}/config_g474.bin
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.list
    # ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT firmware
)