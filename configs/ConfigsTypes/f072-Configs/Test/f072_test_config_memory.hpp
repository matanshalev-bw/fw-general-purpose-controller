/*
 * f072_test_config_memory.hpp
 *
 *  Created on: Jan 16, 2025
 *      Author: ariel
 */

#ifndef SRC_CONFIGSTRUCTS_F072_TEST_CONFIG_MEMORY_HPP_
#define SRC_CONFIGSTRUCTS_F072_TEST_CONFIG_MEMORY_HPP_

#include "config_memory.hpp"
#include "f072_gpio_pins.hpp"

volatile static const FLASH_CONFIG_SECTION ConfigMemory G_CONFIG_READ_ONLY_MEMORY = {
		.config_type = {
				.name = "F072_TEST_CONFIG",
				.type = ConfigTypeEnum::F072_TEST_CONFIG,
		},
		.hardware_config = {
				.mcu_type = McuType::f072,
				.pcb_version = PcbVersion::PCB_VER_2_X,
		},
		.gpio_pin_mapping_config = {
				// Channel ports: [CH_A, CH_B, CH_C, CH_D, CH_E, CH_F, CH_G, CH_H]
				.channel_ports = {
						static_cast<uint8_t>(CH_A_PORT), // CH_A
						static_cast<uint8_t>(CH_B_PORT), // CH_B
						static_cast<uint8_t>(CH_C_PORT), // CH_C
						static_cast<uint8_t>(CH_D_PORT), // CH_D
						static_cast<uint8_t>(CH_E_PORT), // CH_E
						static_cast<uint8_t>(CH_F_PORT), // CH_F
						static_cast<uint8_t>(CH_G_PORT), // CH_G
						static_cast<uint8_t>(CH_H_PORT)  // CH_H
				},
				// Channel pins: [CH_A, CH_B, CH_C, CH_D, CH_E, CH_F, CH_G, CH_H]
				.channel_pins = {
						static_cast<uint8_t>(CH_A_PIN), // CH_A
						static_cast<uint8_t>(CH_B_PIN), // CH_B
						static_cast<uint8_t>(CH_C_PIN), // CH_C
						static_cast<uint8_t>(CH_D_PIN), // CH_D
						static_cast<uint8_t>(CH_E_PIN), // CH_E
						static_cast<uint8_t>(CH_F_PIN), // CH_F
						static_cast<uint8_t>(CH_G_PIN), // CH_G
						static_cast<uint8_t>(CH_H_PIN)  // CH_H
				}
		},
		.verification_config = {
				.verification_bitmask = (1 << VerificationType::VERIFICATION_DIGITAL_OUTPUT) |
				                         (1 << VerificationType::VERIFICATION_TRACTOR_HARNESS),
				.digital_verification_config = {
						// All channels enabled for test config
						.active_input_channels_bitmask = 0xFF,  // All 8 channels (A-H) enabled
						.shuttle_lever_channel = 5, // CH_F
				},
		},
		.sequences_config = {
				.delay_between_steps = 100,
				// Forward sequence steps - each step triggers one channel to go high
				.forward_sequence = {
						.steps = {
								{ // STATE_0 - CH_A = SET
										.gpios = {
												CH_GPIO(CH_A_PORT, CH_A_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_1 - CH_B = SET
										.gpios = {
												CH_GPIO(CH_B_PORT, CH_B_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_2 - CH_C = SET
										.gpios = {
												CH_GPIO(CH_C_PORT, CH_C_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_3 - CH_D = SET
										.gpios = {
												CH_GPIO(CH_D_PORT, CH_D_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_4 - CH_E = SET
										.gpios = {
												CH_GPIO(CH_E_PORT, CH_E_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_5 - CH_F = SET
										.gpios = {
												CH_GPIO(CH_F_PORT, CH_F_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_6 - CH_G = SET
										.gpios = {
												CH_GPIO(CH_G_PORT, CH_G_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_7 - CH_H = SET
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_SET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // End of sequence
										.gpios = {
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								}
						}
				},

				// Reverse sequence steps - each step triggers one channel to go low
				.reverse_sequence = {
						.steps = {
								{ // STATE_0 - CH_A = RESET
										.gpios = {
												CH_GPIO(CH_A_PORT, CH_A_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_1 - CH_B = RESET
										.gpios = {
												CH_GPIO(CH_B_PORT, CH_B_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_2 - CH_C = RESET
										.gpios = {
												CH_GPIO(CH_C_PORT, CH_C_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_3 - CH_D = RESET
										.gpios = {
												CH_GPIO(CH_D_PORT, CH_D_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_4 - CH_E = RESET
										.gpios = {
												CH_GPIO(CH_E_PORT, CH_E_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_5 - CH_F = RESET
										.gpios = {
												CH_GPIO(CH_F_PORT, CH_F_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_6 - CH_G = RESET
										.gpios = {
												CH_GPIO(CH_G_PORT, CH_G_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_7 - CH_H = RESET
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_RESET),
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // End of sequence
										.gpios = {
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								}
						}
				},

				// Neutral sequence steps - do nothing (empty sequence)
				.neutral_sequence = {
						.steps = {
								{ // End of sequence (immediate)
										.gpios = {
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								}
						}
				}
		}
};
#endif /* SRC_CONFIGSTRUCTS_F072_TEST_CONFIG_MEMORY_HPP_ */

