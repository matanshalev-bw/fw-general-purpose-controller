cmake_minimum_required(VERSION 3.25)

# Set the project name and specify languages
project(fw-bootloader LANGUAGES C CXX ASM)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Define the target MCU
set(MCU_FAMILY STM32F0xx)
set(MCU_MODEL STM32F072xB)
set(MCU_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F072C8TX_FLASH.ld")

# Project-specific definitions
add_definitions(-DUSE_HAL_DRIVER -DSTM32F072xB)

# Mixed optimization approach matching IDE: HAL drivers (-O0), Interface files (-Os)
set(CMAKE_C_FLAGS "-mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP --specs=nano.specs -mfloat-abi=soft -mthumb")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m0 -std=gnu++14 -g3 -DDEBUG -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP --specs=nano.specs -mfloat-abi=soft -mthumb")

set(CMAKE_ASM_FLAGS "-mcpu=cortex-m0 -g3 -DDEBUG -c -x assembler-with-cpp -MMD -MP --specs=nano.specs -mfloat-abi=soft -mthumb")

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
    "-T${MCU_LINKER_SCRIPT} -Wl,-Map=${PROJECT_NAME}.map -Wl,--gc-sections -static -mfloat-abi=soft -mthumb -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group"
)

# Include directories in exact IDE order (CRITICAL for correct header resolution)
include_directories(
    Core/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F0xx/Include
    Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rd_party/bluelink_sdk/bluelink_version
    ${CMAKE_CURRENT_SOURCE_DIR}/../configs/ConfigStructs
    ${CMAKE_CURRENT_SOURCE_DIR}/../configs/ConfigStructs/SequencesConfigs
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/MemoryMap
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/NonVolatileMemoryInterface
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/SystemInterface
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/CommInterface
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/CanMessenger
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/SchedulerInterface
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/GpioInterface
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface/f072/InterfaceDefines
)

# HAL Driver sources
# HAL drivers matching IDE build exactly
set(HAL_SOURCES
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal.c"
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_can.c"
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
    Core/Src/bootloader_main.cpp
    Core/Src/stm32f0xx_hal_msp.c
    Core/Src/stm32f0xx_it.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    Core/Src/system_stm32f0xx.c
)

set(INTERFACE_SOURCES
    ../interface/f072/SystemInterface/system_interface.cpp
    ../interface/f072/SchedulerInterface/scheduler_interface.cpp
    ../interface/f072/CommInterface/comm_interface.cpp
    ../interface/f072/NonVolatileMemoryInterface/non_volatile_memory_interface.cpp
    ../interface/f072/CanMessenger/can_messenger.cpp
    ../interface/f072/GpioInterface/gpio_interface.cpp
)

# Startup file
set(STARTUP_ASM Core/Startup/startup_stm32f072c8tx.s)

# Bluelink SDK sources (matching IDE build exactly)
set(BLUELINK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include/PayloadStructs/PayloadTypes.cpp"
)

# Create executable
add_executable(${PROJECT_NAME}.elf
    ${HAL_SOURCES}
    ${CORE_SOURCES}
    ${BLUELINK_SOURCES}
    ${STARTUP_ASM}
    ${INTERFACE_SOURCES}
)

# Apply size optimization (-Os) to interface files to match IDE build exactly
foreach(interface_file ${INTERFACE_SOURCES})
    set_source_files_properties(
        ${interface_file}
        PROPERTIES COMPILE_FLAGS "-Os"
    )
endforeach()

# Apply size optimization (-Os) to core C++ files to match IDE build exactly
set_source_files_properties(
    Core/Src/main.cpp
    Core/Src/bootloader_main.cpp
    PROPERTIES COMPILE_FLAGS "-Os"
)


# Post-build commands to generate hex and bin files
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
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