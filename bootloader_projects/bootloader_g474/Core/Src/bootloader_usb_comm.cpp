#include "bootloader_usb_comm.hpp"

#include <cstring>

#include "bootloader_main.hpp"
#include "usb_comm.hpp"

BootloaderUsbComm* BootloaderUsbComm::instance_ = nullptr;

BootloaderUsbComm::BootloaderUsbComm(BootloaderMain* bootloader_main) : bootloader_main_(bootloader_main) {}

void BootloaderUsbComm::initialize() {
  UsbComm::instance().initialize();

  instance_ = this;

  callbacks_ = std::make_unique<GpcBluelinkUsbCallbacks>(UsbComm::instance().interface(),
                                                       &BootloaderUsbComm::parsePayload);

  bluelink_ = std::make_unique<bluelink::BluelinkCommunicationHandler>(
      bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, callbacks_.get());
}

void BootloaderUsbComm::tick() {
  if (bluelink_ == nullptr) {
    return;
  }

  bluelink_->processReceivedData(process_buffer_);
  bluelink_->writePendingDataIfNeeded();
}

bool BootloaderUsbComm::parsePayload(bluelink::PayloadTypeIds payload_type, const uint8_t* buffer) {
  if (instance_ == nullptr || buffer == nullptr || instance_->bootloader_main_ == nullptr) {
    return false;
  }

  bluelink::Header header{};
  bluelink::Deserializer::decodeHeader(&header, buffer);

  BootloaderInboundMessage message{};
  message.source_id = header.source_id;
  message.payload_type_id = static_cast<uint8_t>(payload_type);
  const size_t payload_size = bluelink::Serializer::GetSizeOfPayload(header);
  message.length = static_cast<uint8_t>(
      payload_size > BootloaderInboundMessage::MAX_PAYLOAD_BYTES ? BootloaderInboundMessage::MAX_PAYLOAD_BYTES
                                                                 : payload_size);
  std::memcpy(message.data, &buffer[sizeof(bluelink::Header)], message.length);

  return instance_->bootloader_main_->handleInboundMessage(message, BootloaderTransport::USB_BLUELINK);
}

bool BootloaderUsbComm::sendProgrammingCommand(uint8_t destination_id,
                                               const bluelink::CommandsPayload::ProgrammingCommand& cmd) {
  if (bluelink_ == nullptr) {
    return false;
  }

  bluelink::Packet<bluelink::CommandsPayload::ProgrammingCommand> packet(
      bluelink::PayloadTypeIds::PROGRAMMING_COMMAND, destination_id);
  packet.payload_data = cmd;
  return bluelink_->writeMessageNow(packet) > 0;
}

bool BootloaderUsbComm::sendControllerMetaData(uint8_t destination_id,
                                               const bluelink::TelemetryPayload::ControllerMetaData& data) {
  if (bluelink_ == nullptr) {
    return false;
  }

  bluelink::Packet<bluelink::TelemetryPayload::ControllerMetaData> packet(
      bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY, destination_id);
  packet.payload_data = data;
  return bluelink_->writeMessageNow(packet) > 0;
}
