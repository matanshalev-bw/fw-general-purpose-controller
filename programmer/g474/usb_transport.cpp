#include "concrete_bluelink_callbacks.hpp"
#include "usb_transport.hpp"

#include <cstring>
#include <unistd.h>

#include "bluelink_communication_handler.hpp"
#include "serial.h"

namespace {

constexpr int kSerialTimeoutMs = 700;
constexpr int kDefaultBaud = 115200;
constexpr int kReopenWaitMs = 2000;

void delayMs(unsigned ms) { usleep(ms * 1000); }

struct UsbReceivedMessage {
  bool has_programming = false;
  bool has_metadata = false;
  uint8_t source_id = 0;
  bluelink::CommandsPayload::ProgrammingCommand programming{};
  bluelink::TelemetryPayload::ControllerMetaData metadata{};
};

UsbReceivedMessage g_usb_received{};

bool parseInbound(bluelink::PayloadTypeIds payload_type, const uint8_t* buffer) {
  if (buffer == nullptr) {
    return false;
  }

  bluelink::Header header{};
  bluelink::Deserializer::decodeHeader(&header, buffer);
  g_usb_received.source_id = header.source_id;

  switch (payload_type) {
    case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
      std::memcpy(&g_usb_received.programming, &buffer[sizeof(bluelink::Header)],
                  sizeof(g_usb_received.programming));
      g_usb_received.has_programming = true;
      return true;

    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
      std::memcpy(&g_usb_received.metadata, &buffer[sizeof(bluelink::Header)],
                  sizeof(g_usb_received.metadata));
      g_usb_received.has_metadata = true;
      return true;

    default:
      return true;
  }
}

}  // namespace

UsbTransport::UsbTransport(std::string port) : port_(std::move(port)) {}

bool UsbTransport::openSerial() {
  if (serial_ == nullptr) {
    serial_ = std::make_unique<serial::Serial>();
  }

  serial_->setPort(port_);
  serial_->setBaudrate(kDefaultBaud);
  serial_->setParity(serial::parity_none);
  serial::Timeout timeout = serial::Timeout::simpleTimeout(kSerialTimeoutMs);
  serial_->setTimeout(timeout);

  try {
    if (serial_->isOpen()) {
      serial_->close();
    }
    serial_->open();
  } catch (const std::exception&) {
    return false;
  }

  return true;
}

bool UsbTransport::init() {
  if (!openSerial()) {
    return false;
  }

  callbacks_ = std::make_unique<ConcreteBluelinkCallbacks>(serial_.get(), parseInbound, false);
  bluelink_ = std::make_unique<bluelink::BluelinkCommunicationHandler>(bluelink::HLC_ADDRESS, callbacks_.get());
  return true;
}

void UsbTransport::shutdown() {
  if (serial_ != nullptr && serial_->isOpen()) {
    serial_->close();
  }
}

bool UsbTransport::reopenAfterReset() {
  shutdown();
  delayMs(kReopenWaitMs);

  for (int attempt = 0; attempt < 20; ++attempt) {
    if (init()) {
      return true;
    }
    delayMs(250);
  }

  return false;
}

bool UsbTransport::processIncoming(int timeout_ms) {
  if (bluelink_ == nullptr) {
    return false;
  }

  const int step_ms = 10;
  int elapsed = 0;
  while (elapsed < timeout_ms) {
    bluelink_->processReceivedData(process_buffer_);
    bluelink_->writePendingDataIfNeeded();
    if (g_usb_received.has_programming || g_usb_received.has_metadata) {
      return true;
    }
    delayMs(step_ms);
    elapsed += step_ms;
  }

  return g_usb_received.has_programming || g_usb_received.has_metadata;
}

bool UsbTransport::requestMetaData(bluelink::ComponentId destination,
                                   bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
  if (bluelink_ == nullptr) {
    return false;
  }

  g_usb_received = UsbReceivedMessage{};

  bluelink::Packet<bluelink::TelemetryPayload::ControllerMetaData> packet(
      bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY, static_cast<uint8_t>(destination));
  packet.payload_data = bluelink::TelemetryPayload::ControllerMetaData{};

  const int tries = 10;
  for (int i = 0; i < tries; ++i) {
    g_usb_received = UsbReceivedMessage{};
    if (bluelink_->writeMessageNow(packet) <= 0) {
      delayMs(150);
      continue;
    }

    if (processIncoming(500) && g_usb_received.has_metadata &&
        g_usb_received.metadata.component_id > 0) {
      meta_data = g_usb_received.metadata;
      return true;
    }
    delayMs(150);
  }

  return false;
}

bool UsbTransport::sendProgrammingCommand(bluelink::ComponentId destination,
                                          const bluelink::CommandsPayload::ProgrammingCommand& cmd) {
  if (bluelink_ == nullptr) {
    return false;
  }

  bluelink::Packet<bluelink::CommandsPayload::ProgrammingCommand> packet(
      bluelink::PayloadTypeIds::PROGRAMMING_COMMAND, static_cast<uint8_t>(destination));
  packet.payload_data = cmd;
  return bluelink_->writeMessageNow(packet) > 0;
}

bool UsbTransport::receiveProgrammingCommand(bluelink::ComponentId expected_source,
                                             bluelink::CommandsPayload::ProgrammingCommand& cmd,
                                             int timeout_ms) {
  g_usb_received = UsbReceivedMessage{};

  if (!processIncoming(timeout_ms)) {
    return false;
  }

  if (!g_usb_received.has_programming || g_usb_received.source_id != static_cast<uint8_t>(expected_source)) {
    return false;
  }

  cmd = g_usb_received.programming;
  return true;
}
