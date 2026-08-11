#include "raw_can_interface.hpp"

#include "main.h"

uint16_t RawCanInterface::can_receive_size_ = 0;
bool RawCanInterface::can_transmit_flag_ = true;

RawCanInterface::RawCanInterface(CommCanHandle* fdcan_handler)
    : comm_can_(std::make_unique<CommCan>(fdcan_handler, &can_receive_size_, &can_transmit_flag_, false)),
      handler_(fdcan_handler) {
  if (initializeCanInterface() != InterfaceStatus::INTERFACE_OK) {
    Error_Handler();
  }
}

InterfaceStatus RawCanInterface::initializeCanInterface() {
  InterfaceStatus status = comm_can_->startCanPeripheral();
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

  comm_can_->kickStartTxInterrupts();
  return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus RawCanInterface::transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc) {
  if (data == nullptr || dlc == 0 || dlc > 8) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  if (not comm_can_->isTransmitAvailable()) {
    comm_can_->recoverStuckTransmit();
    if (not comm_can_->isTransmitAvailable()) {
      return InterfaceStatus::INTERFACE_BUSY;
    }
  }

  comm_can_->setTxCanIdType(CommCan::CanIdType::CAN_ID_TYPE_STD);
  comm_can_->setTxCanId(id & 0x7FFU);
  comm_can_->setTxDlc(dlc);
  comm_can_->setTxRtrType(CommCan::CanRtrType::CAN_RTR_DATA_TYPE);

  return comm_can_->startTransmitInterrupt(data, dlc);
}

InterfaceStatus RawCanInterface::receiveStandard(uint32_t id, uint8_t* data, uint8_t& dlc,
                                                 uint32_t timeout_ms) {
  return CommCan::receiveStandard(handler_, id, data, dlc, timeout_ms);
}
