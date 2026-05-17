#ifndef SRC_CONFIGSTRUCTS_CONFIG_READ_ONLY_MEMORY_HPP_
#define SRC_CONFIGSTRUCTS_CONFIG_READ_ONLY_MEMORY_HPP_

#include "config_defines.hpp"
#include "sequences_configs.hpp"

struct ConfigMemory {
  const char START_SIGN[sizeof(CONFIGS_START_SIGN)] = CONFIGS_START_SIGN;
  const ConfigMemoryVersion VERSION{};
  ConfigType config_type;
  BluelinkIdentityConfig bluelink_identity_config;
  SequencesConfig sequences_config;
  const char END_SIGN[sizeof(CONFIGS_END_SIGN)] = CONFIGS_END_SIGN;
} __attribute__((packed));

#endif  // SRC_CONFIGSTRUCTS_CONFIG_READ_ONLY_MEMORY_HPP_
