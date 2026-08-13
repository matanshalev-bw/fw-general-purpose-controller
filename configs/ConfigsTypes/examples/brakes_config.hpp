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
        .component_id = bluelink::ComponentId::COMPONENT_ID_BRAKES_CONTROLLER,
    },
    .sequences_config = {
        .powerup_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .main_tick_sequence = {
            .step_count = 2,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 1, 0x701, {0x5, 0, 0, 0, 0, 0, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {100},
                        },
            },
        },
        .init_state_sequence = {
            .step_count = 18,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {10, {0, 0, 0}, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {11, {0, 0, 0}, 33},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {12, {0, 0, 0}, 66},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {13, {0, 0, 0}, 100},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {1000},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 2, 0x0, {0x80, 0x20, 0, 0, 0, 0, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x620, {0x2B, 0x15, 0x10, 0, 0xD0, 0x7, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x620, {0x23, 0x16, 0x10, 0x1, 0xF4, 0x1, 0x1, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x620, {0x2B, 0, 0x18, 0x5, 0x32, 0, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x620, {0x2B, 0, 0x40, 0x8, 0x90, 0x1, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 2, 0x0, {0x1, 0x20, 0, 0, 0, 0, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x3, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
                        },
            },
        },
        .manual_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .disengagement_state_sequence = {
            .step_count = 1,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x2, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
                        },
            },
        },
        .engaged_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .power_up_bit_state_sequence = {
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
            .step_count = 1,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x3, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
                        },
            },
        },
        .emergency_state_tick_sequence = {
            .step_count = 1,
            .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x2, 0xFB, 0xFA, 0xC8, 0, 0, 0, 0}},
                        },
            },
        },
        .binding_count = 3,
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
                    .step_count = 3,
                    .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {0, {0, 0, 0}, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {1, {0, 0, 0}, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x2, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
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
                    .step_count = 3,
                    .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {0, {0, 0, 0}, 3},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {1, {0, 0, 0}, 100},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x1, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
                        },
                    },
                },
            },
            {
                .trigger = {
                    .payload_type = bluelink::PayloadTypeIds::BRAKES_CONTINUOUS_COMMAND,
                    .size = 1,
                    .data = {static_cast<uint8_t>(bluelink::BRAKE_MODE_ARMED), 0, 0, 0, 0, 0, 0, 0},
                },
                .extract_field_count = 1,
                .extract_fields = {
                    { 1, 1, 1 },
                },
                .sequence = {
                    .step_count = 10,
                    .steps = {
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::VAR_SET,
                            .var_set = {0, {0, 0, 0}, 2},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::IF_CONDITION,
                            .if_condition = {1, static_cast<uint8_t>(bluelink::MicroOpsPayload::MicroCompareType::LE), 10, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x2, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::IF_CONDITION,
                            .if_condition = {1, static_cast<uint8_t>(bluelink::MicroOpsPayload::MicroCompareType::GE), 13, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0x1, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::IF_CONDITION,
                            .if_condition = {1, static_cast<uint8_t>(bluelink::MicroOpsPayload::MicroCompareType::LE), 11, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0xE9, 0, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::IF_CONDITION,
                            .if_condition = {1, static_cast<uint8_t>(bluelink::MicroOpsPayload::MicroCompareType::LE), 12, 1},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0xD2, 0x1, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0}},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT,
                            .can_transmit = {1, 8, 0x220, {0xBC, 0x2, 0xFB, 0xFB, 0xC8, 0x2, 0x2, 0}},
                        },
                    },
                },
            },
        },
        .telemetry_config = {
            .binding_count = 0,
            .bindings = {
            },
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_