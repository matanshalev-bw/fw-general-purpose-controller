#ifndef SRC_CONFIGSTRUCTS_CONFIG_DEFINES_HPP_
#define SRC_CONFIGSTRUCTS_CONFIG_DEFINES_HPP_

#include <stdint.h>
#include "versions.hpp"

#define FLASH_CONFIG_SECTION __attribute__((used)) __attribute__((section(".config")))

#define CONFIGS_START_SIGN "BW_CONF_S"
#define CONFIGS_END_SIGN "BW_CONF_E"

enum ConfigTypeEnum : uint8_t {
  UNDEFINED_CONFIG = 0,
  GPC_CONFIG = 1,
};

struct ConfigMemoryVersion {
  uint8_t major = CONFIG_READ_ONLY_MEMORY_VERSION_MAJOR;
  uint8_t minor = CONFIG_READ_ONLY_MEMORY_VERSION_MINOR;
  uint8_t patch = CONFIG_READ_ONLY_MEMORY_VERSION_PATCH;
} __attribute__((packed));

struct ConfigType {
  char name[32] = "INVALID CONFIG";
  ConfigTypeEnum type = ConfigTypeEnum::UNDEFINED_CONFIG;
} __attribute__((packed));

#endif  // SRC_CONFIGSTRUCTS_CONFIG_DEFINES_HPP_
