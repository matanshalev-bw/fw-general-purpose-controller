/*
 * meta_data_memory.hpp
 *
 *  Created on: Sep 12, 2024
 *      Author: matan
 */

#ifndef SRC_META_DATA_HPP_
#define SRC_META_DATA_HPP_

#include "distributed_can_id.hpp"
#include "config_defines.hpp"
#include "versions.hpp"

struct ApplicationVersion {
  uint8_t major = APPLICATION_VERSION_MAJOR;
  uint8_t minor = APPLICATION_VERSION_MINOR;
  uint8_t patch = APPLICATION_VERSION_PATCH;
};

struct BootloaderVersion {
  uint8_t major = BOOTLOADER_VERSION_MAJOR;
  uint8_t minor = BOOTLOADER_VERSION_MINOR;
  uint8_t patch = BOOTLOADER_VERSION_PATCH;
};

enum ProgrammingState : uint8_t {
	PROGRAMMING_STATE_WAIT_FOR_PROGRAM = 0,
	PROGRAMMING_STATE_PROGRAMMING_COMMAND,
	PROGRAMMING_STATE_IN_PROGRESS,
	PROGRAMMING_STATE_FLASHED,
};

struct MetaData {
  bluelink::ComponentId MY_COMPONENT_ID = bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER;
  const ApplicationVersion APPLICATION_VERSION{};
  const ConfigMemoryVersion CONFIGURATION_VERSION{};
  ConfigType config_type = {"INVALID CONFIG", ConfigTypeEnum::UNDEFINED_CONFIG};
  ProgrammingState programming_state = ProgrammingState::PROGRAMMING_STATE_FLASHED;
  BootloaderVersion BOOTLOADER_VERSION{};
};

#endif /* SRC_META_DATA_HPP_ */
