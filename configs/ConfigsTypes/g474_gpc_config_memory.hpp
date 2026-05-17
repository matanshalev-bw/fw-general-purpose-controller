#ifndef G474_GPC_CONFIG_MEMORY_HPP_
#define G474_GPC_CONFIG_MEMORY_HPP_

#include "config_memory.hpp"
#include "micro_op_builder.hpp"
#include "distributed_can_id.hpp"
#include "PayloadTypes.hpp"

namespace {
constexpr uint8_t kExampleCanPayload[] = {0x12, 0x34, 0x56, 0x78};
}

volatile static const FLASH_CONFIG_SECTION ConfigMemory G_CONFIG_READ_ONLY_MEMORY = {
    .config_type = {
        .name = "G474_GPC_CONFIG",
        .type = ConfigTypeEnum::GPC_CONFIG,
    },
    .bluelink_identity_config = {
        .component_id = bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER,
    },
    .sequences_config = {
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
                        micro_op_builder::digitalGpioWrite(1, 5, 1),
                        micro_op_builder::adcRead(2, 0, 0, 1),
                        micro_op_builder::dacWriteFromVar(1, 0),
                        micro_op_builder::delayMs(500),
                        micro_op_builder::canTransmit(1, 0x12, 4, kExampleCanPayload),
                    },
                },
            },
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_
