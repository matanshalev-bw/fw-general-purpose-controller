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
  handler_ = std::make_unique<CommInterruptsHandler>(comm_, APP_RX_DATA_SIZE);
  handler_->read(rx_buffer_, rx_buffer_);
  initialized_ = true;
}

bool UsbComm::isHostConnected() const { return host_connection != 0U; }

InterfaceStatus UsbComm::pollRead(uint8_t* data, uint16_t& size) {
  if (data == nullptr) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return handler_->read(rx_buffer_, data, size);
}

InterfaceStatus UsbComm::write(const uint8_t* data, uint16_t size) {
  if (data == nullptr or size == 0U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return handler_->write(tx_buffer_, pending_tx_, data, size);
}
