#ifndef PROGRAMMER_G474_CAN_TRANSPORT_HPP_
#define PROGRAMMER_G474_CAN_TRANSPORT_HPP_

#include <string>

#include "bluelink_transport.hpp"

class CanTransport : public BluelinkTransport {
 public:
  explicit CanTransport(std::string can_interface);

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
  void sendCanMessage(uint32_t can_id, const uint8_t* data, size_t size);
  bool receiveCanMessage(uint32_t expected_addr, uint8_t* data, size_t size);

  std::string can_interface_;
  int can_socket_ = -1;
};

#endif  // PROGRAMMER_G474_CAN_TRANSPORT_HPP_
