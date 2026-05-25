/*
 * usb_comm.hpp
 *
 * USB CDC device init and CommUsb transport (BlueLink uses GpcBluelinkUsbCallbacks).
 */

#ifndef SRC_USB_COMM_HPP_
#define SRC_USB_COMM_HPP_

#include <cstdint>
#include <memory>

#include "comm_interface.hpp"
#include "usbd_cdc_if.h"

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

#endif /* SRC_USB_COMM_HPP_ */
