#include "bluelink_packet_log.hpp"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "bluelink_version.hpp"
#include "ConnectivityPayloadClasses.hpp"
#include "TelemetryPayloadClasses.hpp"
#include "checksum_32.hpp"
#include "packet_struct.hpp"

namespace {

std::string qosName(bluelink::QosTypes qos) {
  switch (qos) {
    case bluelink::QosTypes::REQUIRE_ACK:
      return "ack";
    default:
      return "none";
  }
}

std::string toHex(const uint8_t* data, size_t len) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < len; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(data[i]);
  }
  return oss.str();
}

std::string timestampNow() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  const std::time_t t = clock::to_time_t(now);
  std::tm tm_buf{};
  localtime_r(&t, &tm_buf);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
  return oss.str();
}

std::string formatVersionTriplet(const uint8_t* version, size_t count) {
  if (count == 0) {
    return "?";
  }
  std::ostringstream oss;
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      oss << '.';
    }
    oss << static_cast<unsigned>(version[i]);
  }
  return oss.str();
}

std::string formatPayloadDetails(bluelink::PayloadTypeIds type, const uint8_t* payload, size_t payload_len) {
  switch (type) {
    case bluelink::PayloadTypeIds::LOG: {
      if (payload_len < sizeof(bluelink::ConnectivityPayload::Log)) {
        break;
      }
      bluelink::ConnectivityPayload::Log log{};
      std::memcpy(&log, payload, sizeof(log));
      const char* level = "info";
      switch (log.log_level) {
        case bluelink::ConnectivityPayload::Log::LOG_LEVEL_DEBUG:
          level = "debug";
          break;
        case bluelink::ConnectivityPayload::Log::LOG_LEVEL_WARNING:
          level = "warning";
          break;
        case bluelink::ConnectivityPayload::Log::LOG_LEVEL_ERROR:
          level = "error";
          break;
        default:
          break;
      }
      return std::string("log ") + level + ": " + log.message;
    }
    case bluelink::PayloadTypeIds::ACK_PACKET_RECEIVED:
    case bluelink::PayloadTypeIds::NACK_PACKET_RECEIVED: {
      if (payload_len < sizeof(bluelink::ConnectivityPayload::AckPacketReceived)) {
        break;
      }
      bluelink::ConnectivityPayload::AckPacketReceived ack{};
      std::memcpy(&ack, payload, sizeof(ack));
      std::ostringstream oss;
      oss << (type == bluelink::PayloadTypeIds::ACK_PACKET_RECEIVED ? "ack" : "nack") << " seq="
          << ack.packet_sequence_id << " for_type=" << static_cast<int>(ack.type) << " ("
          << payloadTypeName(ack.type) << ')';
      return oss.str();
    }
    case bluelink::PayloadTypeIds::HORN_TELEMETRY: {
      if (payload_len < sizeof(float)) {
        break;
      }
      bluelink::TelemetryPayload::HornTelemetry horn{};
      const size_t copy_len = std::min(payload_len, sizeof(horn));
      std::memcpy(&horn, payload, copy_len);
      std::ostringstream oss;
      oss << "horn requested=" << horn.requested_horn_time << " remaining=" << horn.remaining_horn_time;
      return oss.str();
    }
    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY: {
      if (payload_len < sizeof(bluelink::TelemetryPayload::ControllerMetaData)) {
        break;
      }
      bluelink::TelemetryPayload::ControllerMetaData meta{};
      std::memcpy(&meta, payload, sizeof(meta));
      std::ostringstream oss;
      oss << "meta component=0x" << std::hex << static_cast<unsigned>(meta.component_id) << std::dec
          << " boot=" << formatVersionTriplet(meta.bootloader_version, 2)
          << " app=" << formatVersionTriplet(meta.app_version, 2)
          << " cfg=" << formatVersionTriplet(meta.config_version, 2) << " cfg_type="
          << static_cast<unsigned>(meta.config_type);
      return oss.str();
    }
    case bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY: {
      if (payload_len < sizeof(bluelink::TelemetryPayload::BluelinkVersionTelemetry)) {
        break;
      }
      bluelink::TelemetryPayload::BluelinkVersionTelemetry version{};
      std::memcpy(&version, payload, sizeof(version));
      std::ostringstream oss;
      oss << "bluelink_version " << static_cast<unsigned>(version.major) << '.'
          << static_cast<unsigned>(version.minor) << '.' << static_cast<unsigned>(version.patch);
      return oss.str();
    }
    default:
      break;
  }

  if (payload_len == 0) {
    return "payload=<empty>";
  }
  return "payload=" + toHex(payload, payload_len);
}

}  // namespace

std::string payloadTypeName(bluelink::PayloadTypeIds type) {
  switch (type) {
    case bluelink::PayloadTypeIds::KEEP_ALIVE:
      return "KEEP_ALIVE";
    case bluelink::PayloadTypeIds::ACK_PACKET_RECEIVED:
      return "ACK_PACKET_RECEIVED";
    case bluelink::PayloadTypeIds::NACK_PACKET_RECEIVED:
      return "NACK_PACKET_RECEIVED";
    case bluelink::PayloadTypeIds::LOG:
      return "LOG";
    case bluelink::PayloadTypeIds::HORN_TELEMETRY:
      return "HORN_TELEMETRY";
    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
      return "CONTROLLER_META_DATA_TELEMETRY";
    case bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY:
      return "BLUELINK_VERSION_TELEMETRY";
    case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
      return "PROGRAMMING_COMMAND";
    case bluelink::PayloadTypeIds::PROGRAMMING_REQUEST:
      return "PROGRAMMING_REQUEST";
    case bluelink::PayloadTypeIds::STOP_PROGRAMMING_COMMAND:
      return "STOP_PROGRAMMING_COMMAND";
    default:
      break;
  }
  std::ostringstream oss;
  oss << "TYPE_" << static_cast<int>(type);
  return oss.str();
}

void BluelinkPacketLog::feed(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  buffer_.insert(buffer_.end(), data, data + len);
  tryParsePackets();
}

void BluelinkPacketLog::tryParsePackets() {
  constexpr size_t kMinPacketSize = sizeof(bluelink::Prefix) + sizeof(bluelink::Header) + sizeof(bluelink::Suffix);

  while (true) {
    size_t start = 0;
    for (; start + 1 < buffer_.size(); ++start) {
      if (buffer_[start] == bluelink::PACKET_SIGN[0] && buffer_[start + 1] == bluelink::PACKET_SIGN[1]) {
        break;
      }
    }
    if (start > 0) {
      buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(start));
    }
    if (buffer_.size() < kMinPacketSize) {
      return;
    }

    const uint8_t body_len = buffer_[bluelink::PACKET_LENGTH_INDEX];
    const size_t total_len = sizeof(bluelink::Prefix) + body_len + sizeof(bluelink::Suffix);
    if (buffer_.size() < total_len) {
      return;
    }

    emitPacket(buffer_.data(), total_len);
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(total_len));
  }
}

void BluelinkPacketLog::emitPacket(const uint8_t* packet, size_t total_len) {
  (void)total_len;
  const auto* prefix = reinterpret_cast<const bluelink::Prefix*>(packet);
  const auto* header = reinterpret_cast<const bluelink::Header*>(packet + sizeof(bluelink::Prefix));
  const uint8_t* payload = packet + sizeof(bluelink::Prefix) + sizeof(bluelink::Header);
  const size_t payload_len =
      prefix->length > sizeof(bluelink::Header) ? prefix->length - sizeof(bluelink::Header) : 0;
  const auto* suffix = reinterpret_cast<const bluelink::Suffix*>(packet + sizeof(bluelink::Prefix) + prefix->length);

  const bool checksum_ok =
      Checksum32::VerifyChecksum(reinterpret_cast<const uint8_t*>(header), prefix->length, suffix->checksum);

  std::ostringstream line;
  line << '[' << timestampNow() << "] RX "
       << payloadTypeName(header->payload_type) << '(' << static_cast<int>(header->payload_type) << ") "
       << "0x" << std::hex << static_cast<unsigned>(header->source_id) << "->0x"
       << static_cast<unsigned>(header->destination_id) << std::dec << " qos=" << qosName(header->qos_type)
       << " seq=" << header->packet_sequence_id;
  if (not checksum_ok) {
    line << " CRC_BAD";
  }
  line << " | " << formatPayloadDetails(header->payload_type, payload, payload_len);
  std::cout << line.str() << std::endl;
}
