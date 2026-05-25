/*
 * usb_comm.hpp
 *
 * USB CDC transport for future BlueLink protocol integration.
 */

#ifndef SRC_USB_COMM_HPP_
#define SRC_USB_COMM_HPP_

#include <cstdint>
#include <memory>
#include <vector>

#include "comm_interrupts_handler.hpp"
#include "comm_interface.hpp"
#include "usbd_cdc_if.h"

class UsbComm {
 public:
  static constexpr size_t BUFFER_SIZE = APP_RX_DATA_SIZE;

  static UsbComm& instance();

  void initialize();
  bool isHostConnected() const;
  InterfaceStatus pollRead(uint8_t* data, uint16_t& size);
  InterfaceStatus write(const uint8_t* data, uint16_t size);

  std::shared_ptr<CommInterface> interface() const { return comm_; }
  CommInterruptsHandler& handler() { return *handler_; }

 private:
  UsbComm() = default;

  std::shared_ptr<CommInterface> comm_;
  std::unique_ptr<CommInterruptsHandler> handler_;
  uint8_t rx_buffer_[BUFFER_SIZE]{};
  uint8_t tx_buffer_[BUFFER_SIZE]{};
  std::vector<uint8_t> pending_tx_;
  bool initialized_ = false;
};

#endif /* SRC_USB_COMM_HPP_ */
