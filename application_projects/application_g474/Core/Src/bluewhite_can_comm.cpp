#include "bluewhite_can_comm.hpp"

#include <cstring>

#include "main.h"
#include "non_volatile_memory_interface.hpp"

uint16_t BluewhiteCanComm::can_receive_size_ = 0;
bool BluewhiteCanComm::can_transmit_flag_ = true;
BluewhiteCanComm* BluewhiteCanComm::interrupt_instance_ = nullptr;

BluewhiteCanComm::BluewhiteCanComm(FDCAN_HandleTypeDef* bluelink_fdcan, MicroSequenceExecutor* sequence_executor,
                                   GpcController* gpc_controller)
    : sequence_executor_(sequence_executor),
      comm_can_(std::make_unique<CommCan>(bluelink_fdcan, &can_receive_size_, &can_transmit_flag_)),
      message_handler_(sequence_executor, comm_can_.get(), gpc_controller) {
  const uint8_t component_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;
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
      static_cast<uint8_t>(bluelink::PayloadTypeIds::CONTROLLER_STATE_COMMAND),
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
    BluewhiteInboundMessage message{};
    message.source_id = current_rx_item_.source_id;
    message.payload_type_id = current_rx_item_.payload_type_id;
    message.length = current_rx_item_.length;
    memcpy(message.data, current_rx_item_.data, sizeof(message.data));

    message_handler_.handleInbound(message);
    messages_processed++;
  }
}

void BluewhiteCanComm::processTxQueue() {
  can_messenger_->processQueueFromTick();

  if (message_handler_.metadataReplyPending()) {
    const bluelink::TelemetryPayload::ControllerMetaData meta_data = message_handler_.buildControllerMetaData();
    if (sendCanMessage(message_handler_.metadataReplyDestination(),
                       bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY, &meta_data, sizeof(meta_data))) {
      message_handler_.clearMetadataReplyPending();
    }
  }

  if (message_handler_.bluelinkVersionReplyPending()) {
    const bluelink::TelemetryPayload::BluelinkVersionTelemetry bluelink_version =
        message_handler_.buildBluelinkVersionTelemetry();
    if (sendCanMessage(message_handler_.bluelinkVersionReplyDestination(),
                       bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY, &bluelink_version,
                       sizeof(bluelink_version))) {
      message_handler_.clearBluelinkVersionReplyPending();
    }
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
