/*
 * g474_nh_open_cabin_config_memory.hpp
 *
 *  Created on: Jan 16, 2025
 *      Author: ariel
 */

#ifndef SRC_CONFIGSTRUCTS_G474_NH_OPEN_CABIN_CONFIG_MEMORY_HPP_
#define SRC_CONFIGSTRUCTS_G474_NH_OPEN_CABIN_CONFIG_MEMORY_HPP_

#include "config_memory.hpp"
#include "g474_gpio_pins.hpp"

volatile static const FLASH_CONFIG_SECTION ConfigMemory G_CONFIG_READ_ONLY_MEMORY = {
		.config_type = {
				.name = "G474_NH_OPEN_CABIN_CONFIG",
				.type = ConfigTypeEnum::G474_NH_OPEN_CABIN_CONFIG,
		},
		.hardware_config = {
				.mcu_type = McuType::G474,
				.pcb_version = PcbVersion::PCB_VER_2_0,
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
				                         (1 << VerificationType::VERIFICATION_LSD) | 
				                         (1 << VerificationType::VERIFICATION_TRANSM_FEEDBACK) | 
				                         (1 << VerificationType::VERIFICATION_DRIVE_GEAR_CONDITION) |
				                         (1 << VerificationType::VERIFICATION_TRACTOR_HARNESS),
				.digital_verification_config = {
						// Active channels for G474 NH Open Cabin - A, B, F, H
						.active_input_channels_bitmask = (1 << AnalogChannelBit::CHANNEL_H) | 
						                                   (1 << AnalogChannelBit::CHANNEL_F) | 
						                                   (1 << AnalogChannelBit::CHANNEL_B) | 
						                                   (1 << AnalogChannelBit::CHANNEL_A),
						.shuttle_lever_channel = AnalogChannelBit::CHANNEL_F, // CH_F
				},
				.lsd_verification_config = {
						.neutral_voltage_threshold = 5.0f,
				},
		},
		.sequences_config = {
				.delay_between_steps = 100,
				// Forward sequence steps
				.forward_sequence = {
						.steps = {
								{ // STATE_0
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_SET), // CH_H = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_1
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_RESET), // CH_H = RESET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_2
										.gpios = {
												CH_GPIO(CH_F_PORT, CH_F_PIN, GpioPinState::PIN_SET), // CH_F = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_3
										.gpios = {
												CH_GPIO(CH_A_PORT, CH_A_PIN, GpioPinState::PIN_SET), // CH_A = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_4
										.gpios = {
												CH_GPIO(CH_A_PORT, CH_A_PIN, GpioPinState::PIN_RESET), // CH_A = RESET
												CH_GPIO(CH_F_PORT, CH_F_PIN, GpioPinState::PIN_RESET), // CH_F = RESET
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

				// Reverse sequence steps
				.reverse_sequence = {
						.steps = {
								{ // STATE_0
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_SET), // CH_H = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_1
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_RESET), // CH_H = RESET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_2
										.gpios = {
												CH_GPIO(CH_F_PORT, CH_F_PIN, GpioPinState::PIN_SET), // CH_F = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_3
										.gpios = {
												CH_GPIO(CH_B_PORT, CH_B_PIN, GpioPinState::PIN_SET), // CH_B = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_4
										.gpios = {
												CH_GPIO(CH_B_PORT, CH_B_PIN, GpioPinState::PIN_RESET), // CH_B = RESET
												CH_GPIO(CH_F_PORT, CH_F_PIN, GpioPinState::PIN_RESET), // CH_F = RESET
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

				// Neutral sequence steps
				.neutral_sequence = {
						.steps = {
								{ // STATE_0
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_SET), // CH_H = SET
												UNUSED_GPIO,
												UNUSED_GPIO,
												UNUSED_GPIO
										}
								},
								{ // STATE_1
										.gpios = {
												CH_GPIO(CH_H_PORT, CH_H_PIN, GpioPinState::PIN_RESET), // CH_H = RESET
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
				}
		}
};
#endif /* SRC_CONFIGSTRUCTS_G474_NH_OPEN_CABIN_CONFIG_MEMORY_HPP_ */
