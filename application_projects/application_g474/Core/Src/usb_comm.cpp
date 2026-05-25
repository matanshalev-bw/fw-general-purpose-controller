#include "usb_comm.hpp"

#include "usb_device.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

UsbComm& UsbComm::instance() {
  static UsbComm usb_comm;
  return usb_comm;
}

void UsbComm::initialize() {
  if (initialized_) {
    return;
  }

  MX_USB_Device_Init();

  comm_ = std::make_shared<CommUsb>(&hUsbDeviceFS, &usb_receive_size, &usb_transmit_flag);
  initialized_ = true;
}

bool UsbComm::isHostConnected() const { return host_connection != 0U; }
