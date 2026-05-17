#ifndef FW_LLC_INTERFACES_MEMORY_MAP_HPP_
#define FW_LLC_INTERFACES_MEMORY_MAP_HPP_

/*
 * This file is used by both the application, bootloader, and flashing script.
 * Please avoid changing variable names unless absolutely necessary.
 */

constexpr static uint32_t FLASH_BW_BOOTLOADER_ADDRESS = 0x08000000;
constexpr static uint32_t FLASH_META_DATA_ADDRESS = 0x08010000;
constexpr static uint32_t FLASH_APPLICATION_ADDRESS = 0x08010800;
constexpr static uint32_t FLASH_CONFIG_ADDRESS = 0x0807F000;

constexpr static uint32_t FLASH_BW_BOOTLOADER_BYTES_SIZE = 0x10000; // 64K in hex
constexpr static uint32_t FLASH_META_DATA_BYTES_SIZE = 0x800; // 2K in hex
constexpr static uint32_t FLASH_APPLICATION_BYTES_SIZE = 0x6E800; // 442K in hex
constexpr static uint32_t FLASH_CONFIG_BYTES_SIZE = 0x1000; // 4K in hex

#endif // FW_LLC_INTERFACES_MEMORY_MAP_HPP_
