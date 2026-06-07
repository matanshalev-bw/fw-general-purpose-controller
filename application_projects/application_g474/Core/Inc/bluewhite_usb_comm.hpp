#ifndef BLUEWHITE_USB_COMM_HPP_
#define BLUEWHITE_USB_COMM_HPP_

#include <cstdint>
#include <memory>

#include "bluewhite_message_handler.hpp"
#include "bluelink_serializer.hpp"
#include "gpc_bluelink_usb_callbacks.hpp"
#include "gpc_controller.hpp"
#include "micro_sequence_executor.hpp"

class CommCan;

class BluewhiteUsbComm {
 public:
  explicit BluewhiteUsbComm(MicroSequenceExecutor* sequence_executor, CommCan* comm_for_bootloader,
                              GpcController* gpc_controller = nullptr);

  void initialize();
  void tick();

  BluewhiteMessageHandler& messageHandler() { return message_handler_; }

 private:
  static bool parsePayload(bluelink::PayloadTypeIds payload_type, const uint8_t* buffer);

  static BluewhiteUsbComm* instance_;

  MicroSequenceExecutor* sequence_executor_ = nullptr;
  BluewhiteMessageHandler message_handler_;
  std::unique_ptr<GpcBluelinkUsbCallbacks> callbacks_;
  std::unique_ptr<bluelink::BluelinkCommunicationHandler> bluelink_;
  uint8_t process_buffer_[GpcBluelinkUsbCallbacks::BUFFER_SIZE]{};
};

#endif  // BLUEWHITE_USB_COMM_HPP_
