/*
 * config_read_only_memory.hpp
 *
 *  Created on: Sep 12, 2024
 *      Author: matan
 */

#ifndef SRC_CONFIGSTRUCTS_CONFIG_READ_ONLY_MEMORY_HPP_
#define SRC_CONFIGSTRUCTS_CONFIG_READ_ONLY_MEMORY_HPP_

#include "config_defines.hpp"
#include "hardware_configs.hpp"
#include "sequences_configs.hpp"

/*
 * This header and its data are not generated in fw-main project.
 * The configs data are taken from other resource which should be flashed separately.
 * This header represents the read-only-memory structure
 * Do not change the order of the following parameters if you don't have to!
 */

struct ConfigMemory {
  const char START_SIGN[sizeof(CONFIGS_START_SIGN)] = CONFIGS_START_SIGN;
  const ConfigMemoryVersion VERSION{};
  ConfigType config_type;
  HardwareConfig hardware_config;
  GpioPinMappingConfig gpio_pin_mapping_config;
  VerificationConfig verification_config;
  SequencesConfig sequences_config;
  const char END_SIGN[sizeof(CONFIGS_END_SIGN)] = CONFIGS_END_SIGN;
} __attribute__((packed));

#endif /* SRC_CONFIGSTRUCTS_CONFIG_READ_ONLY_MEMORY_HPP_ */
