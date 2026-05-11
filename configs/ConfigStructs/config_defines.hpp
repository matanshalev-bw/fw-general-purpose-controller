/*
 * driver_defines.hpp
 *
 *  Created on: Sep 15, 2024
 *      Author: matan
 */

#ifndef SRC_CONFIGSTRUCTS_DRIVER_DEFINES_HPP_
#define SRC_CONFIGSTRUCTS_DRIVER_DEFINES_HPP_

#include <stdint.h>
#include "version.hpp"

#define FLASH_CONFIG_SECTION __attribute__((used)) __attribute__((section(".config")))

#define CONFIGS_START_SIGN "BW_LLC_CONF_S"
#define CONFIGS_END_SIGN "BW_LLC_CONF_E"

enum ConfigTypeEnum : uint8_t {
  UNDEFINED_CONFIG,
  G474_LAB_CONFIG,            // 1
  G474_NH_OPEN_CABIN_CONFIG,  // 2
  G474_NH_CLOSE_CABIN_CONFIG, // 3
  F072_LAB_CONFIG,            // 4
  F072_NH_OPEN_CABIN_CONFIG,  // 5
  F072_NH_CLOSE_CABIN_CONFIG, // 6
  G474_TEST_CONFIG,           // 7
  F072_TEST_CONFIG,           // 8
};

enum VerificationType : uint8_t {
  // Common verification
  VERIFICATION_TRACTOR_HARNESS = 0,
  VERIFICATION_DIGITAL_OUTPUT = 1,
  VERIFICATION_TRANSM_FEEDBACK = 2,
  // Open cabin verification
  VERIFICATION_LSD = 3,
  VERIFICATION_DRIVE_GEAR_CONDITION = 4,
  VERIFICATION_CLUTCH_OUTPUT = 5,
  NUM_VERIFICATION_TYPES = 6
};

enum AnalogChannelBit : uint8_t {
  CHANNEL_A = 0,
  CHANNEL_B = 1,
  CHANNEL_C = 2,
  CHANNEL_D = 3,
  CHANNEL_E = 4,
  CHANNEL_F = 5,
  CHANNEL_G = 6,
  CHANNEL_H = 7
};

struct ConfigMemoryVersion {
  uint8_t major = CONFIG_MEMORY_VERSION_MAJOR;
  uint8_t minor = CONFIG_MEMORY_VERSION_MINOR;
  uint8_t patch = CONFIG_MEMORY_VERSION_PATCH;
} __attribute__((packed));

struct ConfigType {
  char name[32] = "INVALID CONFIG";
  ConfigTypeEnum type = ConfigTypeEnum::UNDEFINED_CONFIG;
} __attribute__((packed));

static constexpr uint8_t MAX_ANALOG_CHANNELS_ = 8;

struct GpioPinMappingConfig {
  uint8_t channel_ports[MAX_ANALOG_CHANNELS_] = {0};  // GpioPortType values for channels A-H
  uint8_t channel_pins[MAX_ANALOG_CHANNELS_] = {0};   // GpioPinNumber values for channels A-H
} __attribute__((packed));

struct DigitalOutputVerificationConfig {
  uint8_t active_input_channels_bitmask = 0x00;
  uint8_t shuttle_lever_channel = 0;
  float active_voltage_threshold = 3.3f;
  float inactive_voltage_threshold = 1.1f;
  float voltage_tolerance = 0.5f;
  uint8_t edge_verification_bitmask = 0xFF;  // 0 = falling edge (PIN_RESET), 1 = rising edge (PIN_SET)
  uint8_t max_retries = 3;
} __attribute__((packed));

struct TransmFeedbackConfig {
  float speed_threshold_rpm = 20.0f;
  uint32_t stabilization_delay_ms = 1000;
  uint8_t max_retries = 3;
} __attribute__((packed));

struct TractorLsdVerificationConfig {
  float neutral_voltage_threshold = 5.0f;
  float forward_reverse_voltage_threshold = 0.0f;
  float voltage_tolerance = 0.5f;
  uint32_t verification_delay_ms = 500;
  uint32_t drive_gear_condition_error_timeout_ms = 3000;
  uint8_t max_retries = 3;
} __attribute__((packed));

struct ClutchOutputVerificationConfig {
  float threshold = 0.5f;
} __attribute__((packed));

struct VerificationConfig {
  uint8_t verification_bitmask = 0x00;
  DigitalOutputVerificationConfig digital_verification_config;
  TransmFeedbackConfig transm_feedback_config;
  TractorLsdVerificationConfig lsd_verification_config;
  ClutchOutputVerificationConfig clutch_output_verification_config;
} __attribute__((packed));

// Helper macro for creating GPIO configurations for any channel
#define CH_GPIO(port, pin, state) {static_cast<GpioPortType>(port), static_cast<GpioPinNumber>(pin), static_cast<GpioPinState>(state)}

#define UNUSED_GPIO {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}

#endif /* SRC_CONFIGSTRUCTS_DRIVER_DEFINES_HPP_ */
