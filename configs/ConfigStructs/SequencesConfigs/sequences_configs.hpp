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
  uint8_t size = 0;
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
  MicroSequence powerup_sequence = {};
  MicroSequence main_tick_sequence = {};
  MicroSequence init_state_sequence = {};
  MicroSequence manual_state_tick_sequence = {};
  MicroSequence disengagement_state_sequence = {};
  MicroSequence engaged_state_tick_sequence = {};
  MicroSequence power_up_bit_state_sequence = {};
  MicroSequence operational_state_tick_sequence = {};
  uint8_t binding_count = 0;
  CommandSequenceBinding bindings[MICRO_SEQUENCE_MAX_BINDINGS] = {};
} __attribute__((packed));

#endif  // FW_SEQUENCESCONFIGUARTIONS_HPP_
