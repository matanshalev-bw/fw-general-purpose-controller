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
        .component_id = bluelink::ComponentId::COMPONENT_ID_HANDBRAKE_CONTROLLER,
    },
    .sequences_config = {
        .powerup_sequence = {
            .step_count = 1,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::MOVE_TO_ERROR_STATE,
                            .move_to_error_state = {{0, 0, 0, 0}},
                        },
            },
        },
        .main_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .init_state_sequence = {
            .step_count = 15,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT,
                            .uart_transmit = {1, 8, {105, 110, 105, 116, 32, 0, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {200},
                        },
            },
        },
        .manual_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .disengagement_state_sequence = {
            .step_count = 2,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT,
                            .uart_transmit = {1, 8, {100, 105, 115, 101, 110, 103, 32, 0}},
                        },
            },
        },
        .engaged_state_tick_sequence = {
            .step_count = 5,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {2000},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT,
                            .uart_transmit = {1, 8, {101, 110, 103, 97, 103, 101, 32, 0}},
                        },
            },
        },
        .power_up_bit_state_sequence = {
            .step_count = 1,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT,
                            .uart_transmit = {1, 8, {112, 111, 119, 101, 114, 117, 112, 32}},
                        },
            },
        },
        .operational_state_tick_sequence = {
            .step_count = 5,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT,
                            .uart_transmit = {1, 8, {111, 112, 101, 114, 32, 0, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
            },
        },
        .error_state_tick_sequence = {
            .step_count = 5,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT,
                            .uart_transmit = {1, 8, {101, 114, 114, 111, 114, 32, 0, 0}},
                        },
            },
        },
        .emergency_state_tick_sequence = {
            .step_count = 4,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {10, {0, 0, 0}, 10},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::IF_CONDITION,
                            .if_condition = {3, static_cast<uint8_t>(bluelink::MicroOpsPayload::MicroCompareType::LT), 10, 2},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {3, {0, 0, 0}, 10},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::PWM_SET,
                            .pwm_set = {40, 0, 1, 3},
                        },
            },
        },
        .binding_count = 2,
        .bindings = {
            {
                .trigger = {
                    .payload_type = bluelink::PayloadTypeIds::BRAKES_CONTINUOUS_COMMAND,
                    .size = 2,
                    .data = {static_cast<uint8_t>(bluelink::BRAKE_MODE_FULLY_RELEASED), 0, 0, 0, 0, 0, 0, 0},
                },
                .extract_field_count = 0,
                .extract_fields = {},
                .sequence = {
                    .step_count = 2,
                    .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {1, {0, 0, 0}, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {2, {0, 0, 0}, 1},
                        },
                    },
                },
            },
            {
                .trigger = {
                    .payload_type = bluelink::PayloadTypeIds::BRAKES_CONTINUOUS_COMMAND,
                    .size = 2,
                    .data = {static_cast<uint8_t>(bluelink::BRAKE_MODE_FULLY_PRESSED), 0, 0, 0, 0, 0, 0, 0},
                },
                .extract_field_count = 0,
                .extract_fields = {},
                .sequence = {
                    .step_count = 2,
                    .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {1, {0, 0, 0}, 3},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {2, {0, 0, 0}, 3},
                        },
                    },
                },
            },
        },
        .telemetry_config = {
            .binding_count = 1,
            .bindings = {
            {
                .payload_type = bluelink::PayloadTypeIds::HANDBRAKE_TELEMETRY,
                .payload_size = 2,
                .rate_hz = 1,
                .field_count = 2,
                .fields = {
                    { 0, 1, 1 },
                    { 1, 1, 2 },
                },
            },
            },
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_