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
        .component_id = bluelink::ComponentId::COMPONENT_ID_THROTTLE_CONTROLLER,
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
        .binding_count = 0,
        .bindings = {
        },
        .telemetry_config = {
            .binding_count = 0,
            .bindings = {
            },
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_
