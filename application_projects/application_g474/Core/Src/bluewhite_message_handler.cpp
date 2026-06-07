#include "bluewhite_message_handler.hpp"

#include <cstring>

#include "main.h"
#include "meta_data.hpp"
#include "non_volatile_memory_interface.hpp"
#include "sequences_configs.hpp"
#include "system_interface.hpp"

BluewhiteMessageHandler::BluewhiteMessageHandler(MicroSequenceExecutor* sequence_executor,
                                                   CommCan* comm_for_bootloader,
                                                   GpcController* gpc_controller)
    : sequence_executor_(sequence_executor),
      comm_for_bootloader_(comm_for_bootloader),
      gpc_controller_(gpc_controller) {}

bool BluewhiteMessageHandler::handleInbound(const BluewhiteInboundMessage& message) {
  const bluelink::PayloadTypeIds payload_type =
      static_cast<bluelink::PayloadTypeIds>(message.payload_type_id);

  switch (payload_type) {
    case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
      processProgrammingCommand(message.data);
      return true;

    case bluelink::PayloadTypeIds::RESET_COMMAND:
      processResetCommand(message.data);
      return true;

    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
      requestMetaDataSend(message.source_id);
      return true;

    case bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY:
      bluelink_version_send_requested_ = true;
      bluelink_version_destination_id_ = message.source_id;
      return true;

    case bluelink::PayloadTypeIds::ACK_PACKET_RECEIVED:
    case bluelink::PayloadTypeIds::NACK_PACKET_RECEIVED:
      return true;

    case bluelink::PayloadTypeIds::CONTROLLER_STATE_COMMAND:
      if (gpc_controller_ != nullptr && message.length >= sizeof(bluelink::CommandsPayload::ControllerStateCommand)) {
        const auto* cmd =
            reinterpret_cast<const bluelink::CommandsPayload::ControllerStateCommand*>(message.data);
        gpc_controller_->handleControllerStateCommand(*cmd);
        return true;
      }
      return false;

    default:
      if (tryExecuteMicroCommand(payload_type, message.data, message.length)) {
        return true;
      }
      return tryStartSequenceForMessage(message.payload_type_id, message.data, message.length);
  }
}

void BluewhiteMessageHandler::processProgrammingCommand(const uint8_t* payload) {
  const auto* received_cmd = reinterpret_cast<const bluelink::CommandsPayload::ProgrammingCommand*>(payload);
  const bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState prog_start =
      bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState::PROGRAMMING_STATE_START;

  if (memcmp(&prog_start, &received_cmd->programming_command_union.programming_command_type,
             sizeof(bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState)) == 0) {
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(
        ProgrammingState::PROGRAMMING_STATE_PROGRAMMING_COMMAND);
    SystemInterface::delay(100);
    if (comm_for_bootloader_ != nullptr) {
      SystemInterface::moveToBootloader(comm_for_bootloader_);
    }
  }
}

void BluewhiteMessageHandler::processResetCommand(const uint8_t* payload) {
  const auto* received_cmd = reinterpret_cast<const bluelink::CommandsPayload::ResetCommand*>(payload);
  const bluelink::CommandsPayload::ResetCommand reset_cmd_ref{};

  if (memcmp(&reset_cmd_ref, received_cmd, sizeof(bluelink::CommandsPayload::ResetCommand)) == 0) {
    SystemInterface::resetController();
  }
}

void BluewhiteMessageHandler::requestMetaDataSend(uint8_t destination_id) {
  metadata_send_requested_ = true;
  metadata_destination_id_ = destination_id;
}

bluelink::TelemetryPayload::ControllerMetaData BluewhiteMessageHandler::buildControllerMetaData() const {
  const volatile MetaData& meta = NonVolatileMemoryInterface::META_DATA_;
  return bluelink::TelemetryPayload::ControllerMetaData{
      .component_id = meta.MY_COMPONENT_ID,
      .bootloader_version = {meta.BOOTLOADER_VERSION.major, meta.BOOTLOADER_VERSION.minor},
      .app_version = {meta.APPLICATION_VERSION.major, meta.APPLICATION_VERSION.minor},
      .config_version = {meta.CONFIGURATION_VERSION.major, meta.CONFIGURATION_VERSION.minor},
      .config_type = meta.config_type.type,
  };
}

bluelink::TelemetryPayload::BluelinkVersionTelemetry BluewhiteMessageHandler::buildBluelinkVersionTelemetry() const {
  return bluelink::TelemetryPayload::BluelinkVersionTelemetry{
      .major = BLUELINK_VERSION_MAJOR,
      .minor = BLUELINK_VERSION_MINOR,
      .patch = BLUELINK_VERSION_PATCH,
  };
}

bool BluewhiteMessageHandler::tryExecuteMicroCommand(bluelink::PayloadTypeIds payload_type, const uint8_t* payload,
                                                     uint8_t length) {
  if (sequence_executor_ == nullptr || payload == nullptr ||
      not bluelink::MicroCommandsPayload::isMicroCommandPayload(payload_type)) {
    return false;
  }

  const uint8_t expected_size = bluelink::MicroCommandsPayload::microCommandWireSize(payload_type);
  if (expected_size == 0 || length < expected_size) {
    return false;
  }

  bluelink::MicroOpsPayload::MicroOpStep step{};
  switch (payload_type) {
    case bluelink::PayloadTypeIds::MICRO_DIGITAL_GPIO_WRITE_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE;
      break;
    case bluelink::PayloadTypeIds::MICRO_DIGITAL_GPIO_READ_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_READ;
      break;
    case bluelink::PayloadTypeIds::MICRO_ADC_READ_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::ADC_READ;
      break;
    case bluelink::PayloadTypeIds::MICRO_DAC_WRITE_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::DAC_WRITE;
      break;
    case bluelink::PayloadTypeIds::MICRO_PWM_SET_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::PWM_SET;
      break;
    case bluelink::PayloadTypeIds::MICRO_DELAY_MS_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::DELAY_MS;
      break;
    case bluelink::PayloadTypeIds::MICRO_CAN_TRANSMIT_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT;
      break;
    case bluelink::PayloadTypeIds::MICRO_UART_TRANSMIT_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT;
      break;
    case bluelink::PayloadTypeIds::MICRO_SPI_TRANSFER_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::SPI_TRANSFER;
      break;
    case bluelink::PayloadTypeIds::MICRO_I2C_WRITE_COMMAND:
      step.op_type = bluelink::MicroOpsPayload::MicroOpType::I2C_WRITE;
      break;
    default:
      return false;
  }

  memcpy(step.params, payload, expected_size);
  return sequence_executor_->executeImmediateOp(step);
}

bool BluewhiteMessageHandler::tryStartSequenceForMessage(uint8_t payload_type_id, const uint8_t* payload,
                                                         uint8_t length) {
  if (sequence_executor_ == nullptr || sequence_executor_->isRunning() || payload == nullptr) {
    return false;
  }

  const volatile SequencesConfig& sequences = NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config;
  for (uint8_t i = 0; i < sequences.binding_count && i < MICRO_SEQUENCE_MAX_BINDINGS; ++i) {
    const volatile CommandSequenceBinding& binding = sequences.bindings[i];
    if (binding.trigger.payload_type != static_cast<bluelink::PayloadTypeIds>(payload_type_id)) {
      continue;
    }

    if (length < sizeof(binding.trigger.data)) {
      continue;
    }

    if (memcmp(payload,
               const_cast<const void*>(static_cast<const volatile void*>(binding.trigger.data)),
               sizeof(binding.trigger.data)) != 0) {
      continue;
    }

    if (sequence_executor_->start(binding.sequence)) {
      return true;
    }
  }

  return false;
}
