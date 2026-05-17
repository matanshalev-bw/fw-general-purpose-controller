#ifndef FW_SEQUENCESCONFIGUARTIONS_HPP_
#define FW_SEQUENCESCONFIGUARTIONS_HPP_

#include <stdint.h>
#include "MicroOpsPayloadClasses.hpp"
#include "PayloadTypes.hpp"

#define MICRO_SEQUENCE_MAX_STEPS 15
#define MICRO_SEQUENCE_MAX_BINDINGS 16
#define MICRO_VAR_SLOT_COUNT 8

struct BluelinkIdentityConfig {
  uint8_t component_id = 0;
} __attribute__((packed));

struct CommandTrigger {
  bluelink::PayloadTypeIds payload_type = bluelink::PayloadTypeIds::UNKNOWN;
  uint8_t data[8] = {};
} __attribute__((packed));

struct MicroSequence {
  uint8_t step_count = 0;
  bluelink::MicroOpsPayload::MicroOpStep steps[MICRO_SEQUENCE_MAX_STEPS] = {};
} __attribute__((packed));

struct CommandSequenceBinding {
  CommandTrigger trigger;
  MicroSequence sequence;
} __attribute__((packed));

struct SequencesConfig {
  uint8_t binding_count = 0;
  CommandSequenceBinding bindings[MICRO_SEQUENCE_MAX_BINDINGS] = {};
} __attribute__((packed));

#endif  // FW_SEQUENCESCONFIGUARTIONS_HPP_
