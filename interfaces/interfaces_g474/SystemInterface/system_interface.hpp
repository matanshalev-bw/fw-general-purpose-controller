/*
 * system_interface.hpp
 *
 *  Created on: Nov 28, 2024
 *      Author: matan
 */

#ifndef SRC_SYSTEM_INTERFACE_HPP_
#define SRC_SYSTEM_INTERFACE_HPP_

#include "stm32g4xx_hal.h"
#include "interface_status.hpp"
#include "memory_map.hpp"

// Forward declaration to avoid circular dependency
class CommCan;

// Vector table variables:
#define VECTOR_TABLE_SIZE 					48 // (31 + 1 + 7 +9) 31 positive vectors, 0 vector, and 7 negative vectors (and extra 9 ? )
#define SYSCFG_CFGR1_MEM_MODE__MAIN_FLASH	0  // x0: Main Flash memory mapped at 0x0000 0000
#define SYSCFG_CFGR1_MEM_MODE__SYSTEM_FLASH	1  // 01: System Flash memory mapped at 0x0000 0000
#define SYSCFG_CFGR1_MEM_MODE__SRAM			3  // 11: Embedded SRAM mapped at 0x0000 0000

extern volatile uint32_t g_pfnVectors[VECTOR_TABLE_SIZE];

class SystemInterface {
 public:
  static uint32_t getTick();
  static void delay(const uint32_t delay_ms);
  static InterfaceStatus resetController();
  static InterfaceStatus setVectorTable();
  static InterfaceStatus hardFault();
  static InterfaceStatus moveToBootloader(CommCan* comm_can = nullptr);
};

#endif /* SRC_H723_SYSTEM_INTERFACE_HPP_ */
