#ifndef G474_GPC_CONFIG_MEMORY_HPP_
#define G474_GPC_CONFIG_MEMORY_HPP_

#include "config_memory.hpp"
#include "distributed_can_id.hpp"
#include "PayloadTypes.hpp"

volatile static const FLASH_CONFIG_SECTION ConfigMemory G_CONFIG_READ_ONLY_MEMORY = {
    .config_type = {
        .name = "G474_GPC_CONFIG",
        .type = ConfigTypeEnum::GPC_CONFIG,
    },
    .bluelink_identity_config = {
        .component_id = bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER,
    },
    .sequences_config = {
        .powerup_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .main_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .init_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .manual_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .disengagement_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .engaged_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .power_up_bit_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .operational_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .error_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .emergency_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .technician_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .binding_count = 1,
        .bindings = {
            {
                .trigger = {
                    .payload_type = bluelink::PayloadTypeIds::DRIVE_COMMAND,
                    .data = {0, static_cast<uint8_t>(bluelink::DRIVE_MODE_BRAKE_NEUTRAL), 0, 0, 0, 0, 0, 0},
                },
                .sequence = {
                    .step_count = 5,
                    .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {1, 5, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::ADC_READ,
                            .adc_read = {2, 0, 0, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DAC_WRITE,
                            .dac_write = {1, 0, 0, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 4, 0x12, {0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0}},
                        },
                    },
                },
            },
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_