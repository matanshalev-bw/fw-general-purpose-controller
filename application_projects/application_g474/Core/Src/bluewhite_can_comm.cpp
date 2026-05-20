#include "bluewhite_can_comm.hpp"

#include <cstring>

#include "main.h"
#include "meta_data.hpp"
#include "non_volatile_memory_interface.hpp"
#include "system_interface.hpp"

uint16_t BluewhiteCanComm::can_receive_size_ = 0;
bool BluewhiteCanComm::can_transmit_flag_ = true;
BluewhiteCanComm* BluewhiteCanComm::interrupt_instance_ = nullptr;

BluewhiteCanComm::BluewhiteCanComm(FDCAN_HandleTypeDef* bluelink_fdcan, MicroSequenceExecutor* sequence_executor)
    : sequence_executor_(sequence_executor) {
  const uint8_t component_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;
  comm_can_ = std::make_unique<CommCan>(bluelink_fdcan, &can_receive_size_, &can_transmit_flag_);
  can_messenger_ = std::make_unique<CanMessenger>(comm_can_.get(), static_cast<bluelink::ComponentId>(component_id));
  interrupt_instance_ = this;

  if (initializeCanInterface() != InterfaceStatus::INTERFACE_OK) {
    Error_Handler();
  }
}

InterfaceStatus BluewhiteCanComm::initializeCanInterface() {
  InterfaceStatus status = configureCanFilter();
  if (status != InterfaceStatus::INTERFACE_OK) {
    return status;
  }

  status = comm_can_->startCanPeripheral();
  if (status != InterfaceStatus::INTERFACE_OK) {
    return status;
  }

  const uint32_t notifications = static_cast<uint32_t>(CommCan::CanNotificationType::RX_FIFO0_MSG_PENDING) |
                                 static_cast<uint32_t>(CommCan::CanNotificationType::RX_FIFO1_MSG_PENDING) |
                                 static_cast<uint32_t>(CommCan::CanNotificationType::TX_FIFO_EMPTY);

  status = comm_can_->activateNotifications(notifications);
  if (status != InterfaceStatus::INTERFACE_OK) {
    return status;
  }

  comm_can_->setTxCompleteCallback([]() {
    if (interrupt_instance_ != nullptr && interrupt_instance_->can_messenger_) {
      interrupt_instance_->can_messenger_->processQueueFromInterrupt();
    }
  });

  comm_can_->setRxMessageCallback([](const FDCAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length) {
    if (interrupt_instance_ != nullptr && interrupt_instance_->can_messenger_) {
      interrupt_instance_->directEnqueueRxFromInterrupt(header, data, length);
    }
  });

  comm_can_->kickStartTxInterrupts();
  return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus BluewhiteCanComm::configureCanFilter() {
  const uint8_t component_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;
  uint8_t filter_index = 0;

  const uint8_t high_priority_payloads[] = {
      static_cast<uint8_t>(bluelink::PayloadTypeIds::RESET_COMMAND),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::PROGRAMMING_COMMAND),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::DRIVE_COMMAND),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::DRIVER_STATE_COMMAND),
  };

  InterfaceStatus status = comm_can_->configHighPriorityFilters(component_id, high_priority_payloads,
                                                                sizeof(high_priority_payloads) / sizeof(high_priority_payloads[0]),
                                                                filter_index);
  if (status != InterfaceStatus::INTERFACE_OK) {
    return status;
  }

  filter_index += sizeof(high_priority_payloads) / sizeof(high_priority_payloads[0]);

  status = comm_can_->configDefaultDestinationFilter(component_id, filter_index);
  if (status != InterfaceStatus::INTERFACE_OK) {
    return status;
  }

  return comm_can_->configGlobalFilter();
}

void BluewhiteCanComm::tick() {
  processRxQueuedMessage();
  processTxQueue();
  if (sequence_executor_ != nullptr) {
    sequence_executor_->tick();
  }
}

void BluewhiteCanComm::directEnqueueRxFromInterrupt(const FDCAN_RxHeaderTypeDef& header, const uint8_t* data,
                                                     uint8_t length) {
  const bluelink::J1939CanIdStruct* rx_id = reinterpret_cast<const bluelink::J1939CanIdStruct*>(&header.Identifier);
  const uint8_t component_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;

  if (rx_id->destination_id != component_id) {
    return;
  }

  if (can_messenger_) {
    can_messenger_->enqueueRxMessageFromInterrupt(header, data, length);
  }
}

void BluewhiteCanComm::processRxQueuedMessage() {
  if (can_messenger_ == nullptr || can_messenger_->isRxQueueEmpty()) {
    return;
  }

  uint8_t messages_processed = 0;
  while (messages_processed < CanMessenger::MAX_RX_PROCESS_PER_TICK_ &&
         can_messenger_->getNextRxMessage(current_rx_item_)) {
    const bluelink::PayloadTypeIds payload_type =
        static_cast<bluelink::PayloadTypeIds>(current_rx_item_.payload_type_id);

    switch (payload_type) {
      case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
        processProgrammingCommand();
        break;

      case bluelink::PayloadTypeIds::RESET_COMMAND:
        processResetCommand();
        break;

      case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
        processMetaDataRequest();
        break;

      case bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY:
        processBluelinkVersionRequest();
        break;

      default:
        tryStartSequenceForMessage(current_rx_item_.payload_type_id, current_rx_item_.data, current_rx_item_.length);
        break;
    }

    messages_processed++;
  }
}

void BluewhiteCanComm::processProgrammingCommand() {
  const bluelink::CommandsPayload::ProgrammingCommand* received_cmd =
      reinterpret_cast<const bluelink::CommandsPayload::ProgrammingCommand*>(current_rx_item_.data);
  const bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState prog_start =
      bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState::PROGRAMMING_STATE_START;

  if (memcmp(&prog_start, &received_cmd->programming_command_union.programming_command_type,
             sizeof(bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState)) == 0) {
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(
        ProgrammingState::PROGRAMMING_STATE_PROGRAMMING_COMMAND);
    SystemInterface::delay(100);
    SystemInterface::moveToBootloader(comm_can_.get());
  }
}

void BluewhiteCanComm::processResetCommand() {
  const bluelink::CommandsPayload::ResetCommand* received_cmd =
      reinterpret_cast<const bluelink::CommandsPayload::ResetCommand*>(current_rx_item_.data);
  const bluelink::CommandsPayload::ResetCommand reset_cmd_ref{};

  if (memcmp(&reset_cmd_ref, received_cmd, sizeof(bluelink::CommandsPayload::ResetCommand)) == 0) {
    SystemInterface::resetController();
  }
}

void BluewhiteCanComm::processMetaDataRequest() {
  requestMetaDataSend(current_rx_item_.source_id);
}

void BluewhiteCanComm::processBluelinkVersionRequest() {
  bluelink_version_send_requested_ = true;
  bluelink_version_destination_id_ = current_rx_item_.source_id;
}

void BluewhiteCanComm::processTxQueue() {
  can_messenger_->processQueueFromTick();
  handlePendingMetadataRequest();
  handlePendingBluelinkVersionRequest();
}

void BluewhiteCanComm::handlePendingMetadataRequest() {
  if (!metadata_send_requested_) {
    return;
  }

  const volatile MetaData& meta = NonVolatileMemoryInterface::META_DATA_;
  const bluelink::TelemetryPayload::ControllerMetaData meta_data = {
      .component_id = meta.MY_COMPONENT_ID,
      .bootloader_version = {meta.BOOTLOADER_VERSION.major, meta.BOOTLOADER_VERSION.minor},
      .app_version = {meta.APPLICATION_VERSION.major, meta.APPLICATION_VERSION.minor},
      .config_version = {meta.CONFIGURATION_VERSION.major, meta.CONFIGURATION_VERSION.minor},
      .config_type = meta.config_type.type,
  };

  if (sendCanMessage(metadata_destination_id_, bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY, &meta_data,
                     sizeof(meta_data))) {
    metadata_send_requested_ = false;
  }
}

void BluewhiteCanComm::handlePendingBluelinkVersionRequest() {
  if (!bluelink_version_send_requested_) {
    return;
  }

  const bluelink::TelemetryPayload::BluelinkVersionTelemetry bluelink_version = {
      .major = BLUELINK_VERSION_MAJOR,
      .minor = BLUELINK_VERSION_MINOR,
      .patch = BLUELINK_VERSION_PATCH,
  };

  if (sendCanMessage(bluelink_version_destination_id_, bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY,
                     &bluelink_version, sizeof(bluelink_version))) {
    bluelink_version_send_requested_ = false;
  }
}

bool BluewhiteCanComm::sendCanMessage(uint8_t destination_id, bluelink::PayloadTypeIds payload_type, const void* data,
                                      size_t data_size) {
  FDCAN_TxHeaderTypeDef tx_header;
  const uint8_t component_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;
  const bluelink::J1939CanIdStruct can_id(static_cast<bluelink::ComponentId>(component_id), destination_id,
                                            payload_type);
  comm_can_->setupTxHeader(tx_header, can_id, data_size);

  return can_messenger_->enqueueTxMessage(tx_header, reinterpret_cast<const uint8_t*>(data), data_size);
}

void BluewhiteCanComm::requestMetaDataSend(uint8_t destination_id) {
  metadata_send_requested_ = true;
  metadata_destination_id_ = destination_id;
}

bool BluewhiteCanComm::tryStartSequenceForMessage(uint8_t payload_type_id, const uint8_t* payload, uint8_t length) {
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

    return sequence_executor_->start(binding.sequence);
  }

  return false;
}
