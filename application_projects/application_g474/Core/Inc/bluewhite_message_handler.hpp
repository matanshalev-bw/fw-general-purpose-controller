#ifndef BLUEWHITE_MESSAGE_HANDLER_HPP_
#define BLUEWHITE_MESSAGE_HANDLER_HPP_

#include <cstdint>

#include "bluelink_messages.hpp"
#include "comm_interface.hpp"
#include "micro_sequence_executor.hpp"

struct BluewhiteInboundMessage {
  static constexpr uint8_t MAX_PAYLOAD_BYTES = bluelink::MicroCommandsPayload::MICRO_COMMAND_MAX_WIRE_BYTES;

  uint8_t source_id = 0;
  uint8_t payload_type_id = 0;
  uint8_t data[MAX_PAYLOAD_BYTES]{};
  uint8_t length = 0;
};

class BluewhiteMessageHandler {
 public:
  BluewhiteMessageHandler(MicroSequenceExecutor* sequence_executor, CommCan* comm_for_bootloader);

  bool handleInbound(const BluewhiteInboundMessage& message);

  bool metadataReplyPending() const { return metadata_send_requested_; }
  bool bluelinkVersionReplyPending() const { return bluelink_version_send_requested_; }
  uint8_t metadataReplyDestination() const { return metadata_destination_id_; }
  uint8_t bluelinkVersionReplyDestination() const { return bluelink_version_destination_id_; }

  void clearMetadataReplyPending() { metadata_send_requested_ = false; }
  void clearBluelinkVersionReplyPending() { bluelink_version_send_requested_ = false; }

  bluelink::TelemetryPayload::ControllerMetaData buildControllerMetaData() const;
  bluelink::TelemetryPayload::BluelinkVersionTelemetry buildBluelinkVersionTelemetry() const;

 private:
  MicroSequenceExecutor* sequence_executor_ = nullptr;
  CommCan* comm_for_bootloader_ = nullptr;

  bool metadata_send_requested_ = false;
  bool bluelink_version_send_requested_ = false;
  uint8_t metadata_destination_id_ = 0;
  uint8_t bluelink_version_destination_id_ = 0;

  void processProgrammingCommand(const uint8_t* payload);
  void processResetCommand(const uint8_t* payload);
  void requestMetaDataSend(uint8_t destination_id);
  bool tryExecuteMicroCommand(bluelink::PayloadTypeIds payload_type, const uint8_t* payload, uint8_t length);
  bool tryStartSequenceForMessage(uint8_t payload_type_id, const uint8_t* payload, uint8_t length);
};

#endif  // BLUEWHITE_MESSAGE_HANDLER_HPP_
