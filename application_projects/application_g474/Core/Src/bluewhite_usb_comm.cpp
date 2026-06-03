#include "bluewhite_usb_comm.hpp"

#include <cstring>

#include "bluelink_messages.hpp"
#include "bluelink_serializer.hpp"
#include "non_volatile_memory_interface.hpp"
#include "usb_comm.hpp"

BluewhiteUsbComm* BluewhiteUsbComm::instance_ = nullptr;

BluewhiteUsbComm::BluewhiteUsbComm(MicroSequenceExecutor* sequence_executor, CommCan* comm_for_bootloader)
    : sequence_executor_(sequence_executor),
      message_handler_(sequence_executor, comm_for_bootloader) {}

void BluewhiteUsbComm::initialize() {
  UsbComm::instance().initialize();

  instance_ = this;

  callbacks_ = std::make_unique<GpcBluelinkUsbCallbacks>(UsbComm::instance().interface(), &BluewhiteUsbComm::parsePayload);

  const uint8_t component_id = NonVolatileMemoryInterface::CONFIG_MEMORY_.bluelink_identity_config.component_id;
  bluelink_ = std::make_unique<bluelink::BluelinkCommunicationHandler>(component_id, callbacks_.get());
}

void BluewhiteUsbComm::tick() {
//  if (bluelink_ == nullptr || not UsbComm::instance().isHostConnected()) {
//    return;
//  }

  bluelink_->processReceivedData(process_buffer_);
  bluelink_->writePendingDataIfNeeded();

  if (message_handler_.metadataReplyPending()) {
    bluelink::Packet<bluelink::TelemetryPayload::ControllerMetaData> packet(
        bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY, message_handler_.metadataReplyDestination());
    packet.payload_data = message_handler_.buildControllerMetaData();
    if (bluelink_->writeMessageNow(packet) > 0) {
      message_handler_.clearMetadataReplyPending();
    }
  }

  if (message_handler_.bluelinkVersionReplyPending()) {
    bluelink::Packet<bluelink::TelemetryPayload::BluelinkVersionTelemetry> packet(
        bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY, message_handler_.bluelinkVersionReplyDestination());
    packet.payload_data = message_handler_.buildBluelinkVersionTelemetry();
    if (bluelink_->writeMessageNow(packet) > 0) {
      message_handler_.clearBluelinkVersionReplyPending();
    }
  }
}

bool BluewhiteUsbComm::parsePayload(bluelink::PayloadTypeIds payload_type, const uint8_t* buffer) {
  if (instance_ == nullptr || buffer == nullptr) {
    return false;
  }

  bluelink::Header header{};
  bluelink::Deserializer::decodeHeader(&header, buffer);

  BluewhiteInboundMessage message{};
  message.source_id = header.source_id;
  message.payload_type_id = static_cast<uint8_t>(payload_type);
  const size_t payload_size = bluelink::Serializer::GetSizeOfPayload(header);
  message.length = static_cast<uint8_t>(
      payload_size > BluewhiteInboundMessage::MAX_PAYLOAD_BYTES ? BluewhiteInboundMessage::MAX_PAYLOAD_BYTES
                                                                : payload_size);
  memcpy(message.data, &buffer[sizeof(bluelink::Header)], message.length);

  return instance_->message_handler_.handleInbound(message);
}
