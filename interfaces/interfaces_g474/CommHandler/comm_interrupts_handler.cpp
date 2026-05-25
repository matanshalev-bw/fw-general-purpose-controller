/*
 * comm_interrupts_handler.cpp
 */

#include "comm_interrupts_handler.hpp"

#include <cstring>

InterfaceStatus CommInterruptsHandler::write(uint8_t* tx_buffer, std::vector<uint8_t>& pending_tx_buffer,
                                             const uint8_t* data, const size_t size) {
  if (comm_->isTransmitAvailable()) {
    if (pending_tx_buffer.empty()) {
      if (size > 0U) {
        std::memcpy(tx_buffer, data, size);
        return comm_->startTransmitInterrupt(tx_buffer, static_cast<uint16_t>(size));
      }
      return InterfaceStatus::INTERFACE_OK;
    }
    if (size + pending_tx_buffer.size() > buffer_size_) {
      const uint16_t size_to_transmit = static_cast<uint16_t>(pending_tx_buffer.size());
      std::memcpy(tx_buffer, pending_tx_buffer.data(), pending_tx_buffer.size());
      const InterfaceStatus status = comm_->startTransmitInterrupt(tx_buffer, size_to_transmit);
      pending_tx_buffer.clear();
      if (size > 0U) {
        pending_tx_buffer.insert(pending_tx_buffer.end(), data, data + size);
      }
      return status;
    }
    std::memcpy(tx_buffer, pending_tx_buffer.data(), pending_tx_buffer.size());
    if (size > 0U) {
      std::memcpy(&tx_buffer[pending_tx_buffer.size()], data, size);
    }
    const uint16_t size_to_transmit = static_cast<uint16_t>(size + pending_tx_buffer.size());
    const InterfaceStatus status = comm_->startTransmitInterrupt(tx_buffer, size_to_transmit);
    pending_tx_buffer.clear();
    return status;
  }
  if (size + pending_tx_buffer.size() > buffer_size_) {
    const InterfaceStatus status =
        comm_->write(pending_tx_buffer.data(), static_cast<uint16_t>(pending_tx_buffer.size()));
    pending_tx_buffer.clear();
    if (status != InterfaceStatus::INTERFACE_OK) {
      return status;
    }
  }
  if (size > 0U) {
    pending_tx_buffer.insert(pending_tx_buffer.end(), data, data + size);
  }
  return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommInterruptsHandler::read(uint8_t* rx_buffer, uint8_t* data, uint16_t& size) {
  size = comm_->getDataReceivedSize();
  if (size > 0U) {
    std::memcpy(data, rx_buffer, size);
  }
  return comm_->startReceiveInterrupt(rx_buffer, static_cast<uint16_t>(buffer_size_));
}

InterfaceStatus CommInterruptsHandler::read(uint8_t* rx_buffer, uint8_t* data) {
  uint16_t size = 0;
  return read(rx_buffer, data, size);
}
