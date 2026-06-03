#ifndef SRC_USB_COMM_HPP_
#define SRC_USB_COMM_HPP_

#include <cstdint>
#include <memory>

#include "comm_interface.hpp"
#include "usbd_cdc_if.h"

extern bool usb_transmit_flag;
extern uint16_t usb_receive_size;
extern uint8_t host_connection;

class UsbComm {
 public:
  static constexpr size_t BUFFER_SIZE = APP_RX_DATA_SIZE;

  static UsbComm& instance();

  void initialize();
  bool isHostConnected() const;

  std::shared_ptr<CommInterface> interface() const { return comm_; }

 private:
  UsbComm() = default;

  std::shared_ptr<CommInterface> comm_;
  bool initialized_ = false;
};

#endif  // SRC_USB_COMM_HPP_
