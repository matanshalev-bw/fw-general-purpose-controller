#include "bluewhite_can_comm.hpp"

#include <cstring>

#include "main.h"
#include "non_volatile_memory_interface.hpp"
#include "PayloadTypes.hpp"

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
      static_cast<uint8_t>(bluelink::PayloadTypeIds::DRIVE_COMMAND),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::DRIVER_STATE_COMMAND),
      static_cast<uint8_t>(bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY),
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
    tryStartSequenceForMessage(current_rx_item_.payload_type_id, current_rx_item_.data, current_rx_item_.length);
    messages_processed++;
  }
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
