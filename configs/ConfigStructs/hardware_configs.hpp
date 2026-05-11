/*
 * hardware_configs.hpp
 *
 *  Created on: Jan 15, 2025
 *      Author: ariel
 */

#ifndef SRC_CONFIGSTRUCTS_HARDWARE_CONFIGS_HPP_
#define SRC_CONFIGSTRUCTS_HARDWARE_CONFIGS_HPP_

#include <stdint.h>

enum McuType : uint16_t {
  UNKNOWN_MCU = 0,
  f072 = 72,
  G474 = 474
};

enum PcbVersion : uint8_t {
  UNDEFINED_PCB,
  PCB_VER_1_1,
  PCB_VER_2_X,
  PCB_VER_2_0,
};

struct HardwareConfig {
  McuType mcu_type = McuType::UNKNOWN_MCU;
  PcbVersion pcb_version = PcbVersion::UNDEFINED_PCB;
} __attribute__((packed));

#endif /* SRC_CONFIGSTRUCTS_HARDWARE_CONFIGS_HPP_ */

