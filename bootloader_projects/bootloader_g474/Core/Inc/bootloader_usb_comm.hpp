#ifndef BOOTLOADER_USB_COMM_HPP_
#define BOOTLOADER_USB_COMM_HPP_

#include <cstdint>
#include <memory>

#include "bluelink_serializer.hpp"
#include "gpc_bluelink_usb_callbacks.hpp"

class BootloaderMain;

class BootloaderUsbComm {
 public:
  explicit BootloaderUsbComm(BootloaderMain* bootloader_main);

  void initialize();
  void tick();

  bool sendProgrammingCommand(uint8_t destination_id,
                              const bluelink::CommandsPayload::ProgrammingCommand& cmd);
  bool sendControllerMetaData(uint8_t destination_id,
                              const bluelink::TelemetryPayload::ControllerMetaData& data);

 private:
  static bool parsePayload(bluelink::PayloadTypeIds payload_type, const uint8_t* buffer);

  static BootloaderUsbComm* instance_;

  BootloaderMain* bootloader_main_ = nullptr;
  std::unique_ptr<GpcBluelinkUsbCallbacks> callbacks_;
  std::unique_ptr<bluelink::BluelinkCommunicationHandler> bluelink_;
  uint8_t process_buffer_[GpcBluelinkUsbCallbacks::BUFFER_SIZE]{};
};

#endif  // BOOTLOADER_USB_COMM_HPP_
