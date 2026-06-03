#include "gpc_bluelink_usb_callbacks.hpp"

#include <vector>

GpcBluelinkUsbCallbacks::GpcBluelinkUsbCallbacks(
    std::shared_ptr<CommInterface> comm, HandlePayloadParsinMethodFunctionType parse_payload)
    : CommInterruptsHandler(comm, BUFFER_SIZE), BluelinkCallbacks(parse_payload) {}

size_t GpcBluelinkUsbCallbacks::write(const uint8_t* data, const size_t& size) {
  static uint8_t tx_buffer[BUFFER_SIZE]{};
  static std::vector<uint8_t> pending_tx_buffer{};

  if (CommInterruptsHandler::write(tx_buffer, pending_tx_buffer, data, size) == InterfaceStatus::INTERFACE_OK) {
    return size;
  }
  return 0;
}

uint16_t GpcBluelinkUsbCallbacks::read(uint8_t* data) {
  static uint8_t rx_buffer[BUFFER_SIZE]{};
  uint16_t read_size = 0;

  if (CommInterruptsHandler::read(rx_buffer, data, read_size) == InterfaceStatus::INTERFACE_OK) {
    return read_size;
  }
  return 0;
}
