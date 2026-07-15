#include "gpc_telemetry_sender.hpp"

#include <cstring>

#include "bluelink_messages.hpp"
#include "bluewhite_can_comm.hpp"
#include "bluewhite_usb_comm.hpp"
#include "distributed_can_id.hpp"
#include "gpc_controller.hpp"
#include "micro_var_store.hpp"
#include "non_volatile_memory_interface.hpp"

void GpcTelemetrySender::initialize(MicroVarStore* var_store, BluewhiteCanComm* can_comm,
                                    BluewhiteUsbComm* usb_comm, GpcController* gpc_controller) {
  var_store_ = var_store;
  can_comm_ = can_comm;
  usb_comm_ = usb_comm;
  gpc_controller_ = gpc_controller;

  for (uint8_t i = 0; i < MAX_TELEMETRY_BINDINGS; ++i) {
    schedulers_[i].reset();
  }
  controller_state_scheduler_ =
      std::make_unique<SchedulerMainClock>(static_cast<float>(CONTROLLER_STATE_TELEMETRY_RATE_HZ));

  const volatile TelemetryConfig& telemetry_config =
      NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config.telemetry_config;
  for (uint8_t i = 0; i < telemetry_config.binding_count && i < MAX_TELEMETRY_BINDINGS; ++i) {
    const uint8_t rate_hz = telemetry_config.bindings[i].rate_hz;
    if (rate_hz > 0) {
      schedulers_[i] = std::make_unique<SchedulerMainClock>(static_cast<float>(rate_hz));
    }
  }
}

void GpcTelemetrySender::buildPayload(const volatile TelemetryBinding& binding, uint8_t* out) const {
  memset(out, 0, binding.payload_size);
  if (var_store_ == nullptr) {
    return;
  }

  for (uint8_t i = 0; i < binding.field_count && i < MAX_TELEMETRY_FIELD_MAPPINGS; ++i) {
    const volatile TelemetryFieldMapping& field = binding.fields[i];
    if (field.byte_offset + field.byte_size > binding.payload_size) {
      continue;
    }

    const uint64_t var_value = var_store_->get(field.var_index);
    uint8_t encoded[8] = {};
    switch (field.byte_size) {
      case 1:
        encoded[0] = static_cast<uint8_t>(var_value);
        break;
      case 2: {
        const uint16_t value16 = static_cast<uint16_t>(var_value);
        memcpy(encoded, &value16, sizeof(value16));
        break;
      }
      case 4: {
        const uint32_t value32 = static_cast<uint32_t>(var_value);
        memcpy(encoded, &value32, sizeof(value32));
        break;
      }
      case 8:
        memcpy(encoded, &var_value, sizeof(var_value));
        break;
      default:
        continue;
    }
    memcpy(out + field.byte_offset, encoded, field.byte_size);
  }
}

bool GpcTelemetrySender::sendBinding(const volatile TelemetryBinding& binding, const uint8_t* payload) {
  if (can_comm_ == nullptr || usb_comm_ == nullptr) {
    return false;
  }

  const auto payload_type = static_cast<bluelink::PayloadTypeIds>(binding.payload_type);
  //const uint8_t broadcast_dest = bluelink::ComponentId::COMPONENT_ID_BROADCAST;
  // const bool can_sent =
  //     can_comm_->sendTelemetry(broadcast_dest, payload_type, payload, binding.payload_size);
  const bool usb_sent = usb_comm_->sendTelemetry(payload_type, payload, binding.payload_size);
  //return can_sent && usb_sent;
  return usb_sent;
}

void GpcTelemetrySender::tickControllerStateTelemetry() {
  if (controller_state_scheduler_ == nullptr || not controller_state_scheduler_->isDue() ||
      gpc_controller_ == nullptr || can_comm_ == nullptr || usb_comm_ == nullptr) {
    return;
  }

  bluelink::TelemetryPayload::ControllerStateTelemetry telemetry{};
  telemetry.controller_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;
  telemetry.controller_state = gpc_controller_->getState();

  const auto* payload = reinterpret_cast<const uint8_t*>(&telemetry);
  constexpr uint8_t payload_size = sizeof(telemetry);
  const auto payload_type = bluelink::PayloadTypeIds::CONTROLLER_STATE_TELEMETRY;
  const uint8_t broadcast_dest = bluelink::ComponentId::COMPONENT_ID_BROADCAST;

  const bool usb_sent = usb_comm_->sendTelemetry(payload_type, payload, payload_size);
  const bool can_sent = can_comm_->sendTelemetry(broadcast_dest, payload_type, payload, payload_size);
  if (usb_sent || can_sent) {
    controller_state_scheduler_->restart();
  }
}

void GpcTelemetrySender::tick() {
  if (can_comm_ == nullptr || usb_comm_ == nullptr) {
    return;
  }

  tickControllerStateTelemetry();

  if (var_store_ == nullptr) {
    return;
  }

  const volatile TelemetryConfig& telemetry_config =
      NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config.telemetry_config;
  if (telemetry_config.binding_count == 0) {
    return;
  }

  uint8_t payload[8] = {};
  for (uint8_t i = 0; i < telemetry_config.binding_count && i < MAX_TELEMETRY_BINDINGS; ++i) {
    if (schedulers_[i] == nullptr || not schedulers_[i]->isDue()) {
      continue;
    }

    const volatile TelemetryBinding& binding = telemetry_config.bindings[i];
    if (binding.payload_size == 0 || binding.rate_hz == 0) {
      continue;
    }

    buildPayload(binding, payload);
    if (sendBinding(binding, payload)) {
      schedulers_[i]->restart();
    }
  }
}
