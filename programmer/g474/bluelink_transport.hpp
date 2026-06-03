#ifndef PROGRAMMER_G474_BLUELINK_TRANSPORT_HPP_
#define PROGRAMMER_G474_BLUELINK_TRANSPORT_HPP_

#include "bluelink_messages.hpp"
#include "distributed_can_id.hpp"

class BluelinkTransport {
 public:
  virtual ~BluelinkTransport() = default;

  virtual bool init() = 0;
  virtual void shutdown() = 0;
  virtual bool reopenAfterReset() = 0;

  virtual bool requestMetaData(bluelink::ComponentId destination,
                               bluelink::TelemetryPayload::ControllerMetaData& meta_data) = 0;
  virtual bool sendProgrammingCommand(bluelink::ComponentId destination,
                                      const bluelink::CommandsPayload::ProgrammingCommand& cmd) = 0;
  virtual bool receiveProgrammingCommand(bluelink::ComponentId expected_source,
                                         bluelink::CommandsPayload::ProgrammingCommand& cmd,
                                         int timeout_ms) = 0;
};

#endif  // PROGRAMMER_G474_BLUELINK_TRANSPORT_HPP_
