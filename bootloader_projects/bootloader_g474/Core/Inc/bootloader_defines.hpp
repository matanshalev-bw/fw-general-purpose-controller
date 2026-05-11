/*
 * bootloader_defines.hpp
 */

#ifndef SRC_BOOTLOADER_DEFINES_HPP_
#define SRC_BOOTLOADER_DEFINES_HPP_

#include <stdint.h>

// Define the bootloader state enum here to be shared between modules
enum class BootloaderState : uint8_t {
    INIT,
    WAITING_FOR_COMMAND,
    PROGRAMMING_READY,
    WAITING_FOR_PROGRAMMING_READY,
    PROGRAMMING_IN_PROGRESS,
    PROGRAMMING_COMPLETE,
    ERROR_STATE,
    JUMP_TO_APP
};

#endif /* SRC_BOOTLOADER_DEFINES_HPP_ */
