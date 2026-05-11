#ifndef SRC_H723_NON_VOLATILE_MEMORY_INTERFACE_HPP_
#define SRC_H723_NON_VOLATILE_MEMORY_INTERFACE_HPP_

#include <string.h>

#include "stm32g4xx_hal.h"

#include "config_memory.hpp"
#include "meta_data.hpp"
#include "memory_map.hpp"
#include "interface_status.hpp"

#define FLASH_CONFIG_SECTION __attribute__((used)) __attribute__((section(".config")))
#define FLASH_META_DATA_SECTION __attribute__((used)) __attribute__((section(".meta_data")))

class NonVolatileMemoryInterface {
 public:
  volatile static const FLASH_CONFIG_SECTION ConfigMemory CONFIG_MEMORY_;
  volatile static const FLASH_META_DATA_SECTION MetaData META_DATA_;
  static bool is_config_memory_valid;

 private:
  inline static bool isConfigMemoryExist();
  inline static bool isConfigMemoryVersionOk();
  inline static bool isConfigMemoryFullyPresent();
  static InterfaceStatus writeDataToFlash(const void* data, size_t size);

  public:
  static bool isConfigMemoryValid();
  static InterfaceStatus rewriteMetaData();
  static InterfaceStatus updateProgrammingStateOnMetaData(const ProgrammingState new_state);
  
  // Flash operations for bootloader
  static uint32_t getPage(uint32_t addr);
  static InterfaceStatus unlockFlash(bool unlock = true);
  static InterfaceStatus eraseFlash(uint32_t start_address, uint32_t size);
  static InterfaceStatus eraseFlashBank(uint32_t start_address, uint32_t size, uint32_t bank);
  static InterfaceStatus flashProgram(uint32_t address, uint64_t data);
};

#endif /* SRC_H723_NON_VOLATILE_MEMORY_INTERFACE_HPP_ */
