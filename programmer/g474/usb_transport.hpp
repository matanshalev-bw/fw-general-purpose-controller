#ifndef PROGRAMMER_G474_USB_TRANSPORT_HPP_
#define PROGRAMMER_G474_USB_TRANSPORT_HPP_

#include <memory>
#include <string>

#include "bluelink_communication_handler.hpp"
#include "bluelink_transport.hpp"
#include "concrete_bluelink_callbacks.hpp"
#include "serial.h"

class UsbTransport : public BluelinkTransport {
 public:
  explicit UsbTransport(std::string port);

  bool init() override;
  void shutdown() override;
  bool reopenAfterReset() override;

  bool requestMetaData(bluelink::ComponentId destination,
                       bluelink::TelemetryPayload::ControllerMetaData& meta_data) override;
  bool sendProgrammingCommand(bluelink::ComponentId destination,
                              const bluelink::CommandsPayload::ProgrammingCommand& cmd) override;
  bool receiveProgrammingCommand(bluelink::ComponentId expected_source,
                                 bluelink::CommandsPayload::ProgrammingCommand& cmd,
                                 int timeout_ms) override;

 private:
  bool processIncoming(int timeout_ms);
  bool openSerial();

  std::string port_;
  std::unique_ptr<serial::Serial> serial_;
  std::unique_ptr<ConcreteBluelinkCallbacks> callbacks_;
  std::unique_ptr<bluelink::BluelinkCommunicationHandler> bluelink_;
  uint8_t process_buffer_[255]{};
};

#endif  // PROGRAMMER_G474_USB_TRANSPORT_HPP_
