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
            .step_count = 4,
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
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE,
                            .digital_gpio_write = {2, 15, 0},
                        },
                        {
                            .op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS,
                            .delay_ms = {500},
                        },
            },
        },
        .init_state_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .manual_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .disengagement_state_sequence = {
            .step_count = 0,
            .steps = {
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
        .binding_count = 0,
        .bindings = {
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_