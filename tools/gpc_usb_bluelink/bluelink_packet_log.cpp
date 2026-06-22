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

template <typename T>
bool copyPayloadStruct(const uint8_t* payload, size_t payload_len, T& out) {
  if (payload_len < sizeof(T)) {
    return false;
  }
  std::memcpy(&out, payload, sizeof(T));
  return true;
}

template <typename T, typename Formatter>
bool tryFormatTelemetryPayload(const uint8_t* payload, size_t payload_len, std::ostringstream& oss, Formatter formatter) {
  T value{};
  if (not copyPayloadStruct(payload, payload_len, value)) {
    return false;
  }
  formatter(value, oss);
  return true;
}

std::string formatPayloadDetails(bluelink::PayloadTypeIds type, const uint8_t* payload, size_t payload_len) {
  std::ostringstream oss;
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
    case bluelink::PayloadTypeIds::HORN_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::HornTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::HornTelemetry& horn, std::ostringstream& out) {
                out << "horn requested=" << horn.requested_horn_time << " remaining=" << horn.remaining_horn_time;
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::POWER_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::PowerTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::PowerTelemetry& power, std::ostringstream& out) {
                out << "power battery_mv=" << power.battery_voltage_mv << " vehicle_battery_percent="
                    << static_cast<unsigned>(power.vehicle_battery_level_percent);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY: {
      if (payload_len < sizeof(bluelink::TelemetryPayload::ControllerMetaData)) {
        break;
      }
      bluelink::TelemetryPayload::ControllerMetaData meta{};
      std::memcpy(&meta, payload, sizeof(meta));
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
      oss << "bluelink_version " << static_cast<unsigned>(version.major) << '.'
          << static_cast<unsigned>(version.minor) << '.' << static_cast<unsigned>(version.patch);
      return oss.str();
    }
    case bluelink::PayloadTypeIds::DRIVE_CONTROL_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::DriveControlTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::DriveControlTelemetry& v, std::ostringstream& out) {
                out << "drive autonomous_desired=" << static_cast<unsigned>(v.is_autonomous_desired)
                    << " autonomous=" << static_cast<unsigned>(v.is_autonomous)
                    << " desired_mode=" << static_cast<unsigned>(v.desired_drive_mode)
                    << " actual_mode=" << static_cast<unsigned>(v.actual_drive_mode);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::FOUR_WHEEL_DRIVE_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::FourWheelDriveTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::FourWheelDriveTelemetry& v, std::ostringstream& out) {
                out << "4wd desired=" << static_cast<unsigned>(v.desired_four_wheel_drive_mode)
                    << " actual=" << static_cast<unsigned>(v.actual_four_wheel_drive_mode);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::LLC_STATE_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::LlcStateTelemetry>(
              payload, payload_len, oss, [](const bluelink::TelemetryPayload::LlcStateTelemetry& v,
                                            std::ostringstream& out) {
                out << "llc_state=" << static_cast<unsigned>(v.llc_state);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::LLC_SYSTEM_CONFIG_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::LlcSystemConfigTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::LlcSystemConfigTelemetry& v, std::ostringstream& out) {
                out << "llc_config safety_bumper=" << static_cast<unsigned>(v.safety_bumper_enabled)
                    << " component_version=" << formatVersionTriplet(
                           reinterpret_cast<const uint8_t*>(v.component_version), sizeof(v.component_version))
                    << " tractor_type=" << static_cast<unsigned>(v.tractor_type);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::PPI_PP_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::PowerPanelComponentTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::PowerPanelComponentTelemetry& v, std::ostringstream& out) {
                out << "ppi component_type=" << static_cast<unsigned>(v.component_type)
                    << " efuse_enabled=" << static_cast<unsigned>(v.is_efuse_enabled)
                    << " efuse_faulted=" << static_cast<unsigned>(v.is_efuse_faulted)
                    << " current_sense=" << v.current_sense;
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::PTO_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::PtoTelemetry>(
              payload, payload_len, oss, [](const bluelink::TelemetryPayload::PtoTelemetry& v, std::ostringstream& out) {
                out << "pto desired_mode=" << static_cast<unsigned>(v.desired_pto_mode)
                    << " actual_mode=" << static_cast<unsigned>(v.actual_pto_mode)
                    << " desired_rpm=" << v.desired_rpm << " actual_rpm=" << v.actual_rpm;
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::RAW_SENSORS_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::RawSensorTelemetries>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::RawSensorTelemetries& v, std::ostringstream& out) {
                out << "raw_sensors emergency=" << static_cast<unsigned>(v.emergency_button_pressed)
                    << " bumper_l=" << static_cast<unsigned>(v.bumper_left_pressed)
                    << " bumper_r=" << static_cast<unsigned>(v.bumper_right_pressed)
                    << " system_switch=" << static_cast<unsigned>(v.system_switch_mode);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::REVERSER_ANALOG_CHANNEL_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::ReverserAnalogChannelTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::ReverserAnalogChannelTelemetry& v, std::ostringstream& out) {
                out << "reverser_analog mask=" << static_cast<unsigned>(v.active_channel_mask)
                    << " seq=" << static_cast<unsigned>(v.message_sequence) << " ch_mv=["
                    << v.channel_values_mv[0] << ',' << v.channel_values_mv[1] << ','
                    << v.channel_values_mv[2] << ']';
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::REVERSER_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::ReverserTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::ReverserTelemetry& v, std::ostringstream& out) {
                out << "reverser desired_gear=" << static_cast<unsigned>(v.desired_reverser_gear_mode)
                    << " actual_gear=" << static_cast<unsigned>(v.actual_reverser_gear_mode)
                    << " driver_state=" << static_cast<unsigned>(v.actual_driver_state)
                    << " status_flags=" << v.status_flags
                    << " active_channels=" << static_cast<unsigned>(v.active_channels)
                    << " shuttle_ch=" << static_cast<unsigned>(v.shuttle_lever_channel);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::SEAT_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::SeatTelemetry>(
              payload, payload_len, oss, [](const bluelink::TelemetryPayload::SeatTelemetry& v, std::ostringstream& out) {
                out << "seat desired=" << static_cast<unsigned>(v.desired_seat_mode)
                    << " actual=" << static_cast<unsigned>(v.actual_seat_mode);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::SPRAYERS_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::SprayersTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::SprayersTelemetry& v, std::ostringstream& out) {
                out << "sprayers left=" << static_cast<unsigned>(v.left_is_activated) << '/'
                    << static_cast<unsigned>(v.left_desired_activated) << " right="
                    << static_cast<unsigned>(v.right_is_activated) << '/'
                    << static_cast<unsigned>(v.right_desired_activated) << " power="
                    << static_cast<unsigned>(v.power_is_activated) << '/'
                    << static_cast<unsigned>(v.power_desired_activated) << " master="
                    << static_cast<unsigned>(v.master_is_activated) << '/'
                    << static_cast<unsigned>(v.master_desired_activated);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::STROBE_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::StrobeTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::StrobeTelemetry& v, std::ostringstream& out) {
                out << "strobe desired=" << static_cast<unsigned>(v.desired_strobe_mode)
                    << " actual=" << static_cast<unsigned>(v.actual_strobe_mode);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::THREE_POINT_HITCH_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::ThreePointHitchTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::ThreePointHitchTelemetry& v, std::ostringstream& out) {
                out << "3pt desired_pos=" << v.desired_position << " actual_pos=" << v.actual_position;
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TRANSM_OUT_SPD_TELEMETRY:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TransmOutSpdTelemetry>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TransmOutSpdTelemetry& v, std::ostringstream& out) {
                out << "transm_out_spd=" << v.transm_out_spd;
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_HEARTBEAT_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestHeartbeatResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestHeartbeatResponse& v, std::ostringstream& out) {
                out << "test_heartbeat success=" << static_cast<unsigned>(v.success)
                    << " hlc_comm=" << static_cast<unsigned>(v.hlc_comm_status)
                    << " can_comm=" << static_cast<unsigned>(v.can_comm_status)
                    << " sbus_failsafe=" << static_cast<unsigned>(v.sbus_failsafe);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_FW_VERSION_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestFwVersionResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestFwVersionResponse& v, std::ostringstream& out) {
                out << "test_fw_version success=" << static_cast<unsigned>(v.success) << " version="
                    << static_cast<unsigned>(v.major) << '.' << static_cast<unsigned>(v.minor) << '.'
                    << static_cast<unsigned>(v.patch);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_DIGITAL_WRITE_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestDigitalWriteResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestDigitalWriteResponse& v, std::ostringstream& out) {
                out << "test_digital_write success=" << static_cast<unsigned>(v.success)
                    << " value_written=" << static_cast<unsigned>(v.value_written)
                    << " error_code=" << static_cast<unsigned>(v.error_code);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_DIGITAL_READ_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestDigitalReadResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestDigitalReadResponse& v, std::ostringstream& out) {
                out << "test_digital_read success=" << static_cast<unsigned>(v.success)
                    << " value_read=" << static_cast<unsigned>(v.value_read)
                    << " error_code=" << static_cast<unsigned>(v.error_code);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_ANALOG_READ_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestAnalogReadResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestAnalogReadResponse& v, std::ostringstream& out) {
                out << "test_analog_read success=" << static_cast<unsigned>(v.success)
                    << " error_code=" << static_cast<unsigned>(v.error_code) << " voltage=" << v.voltage;
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_EFUSE_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestEfuseResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestEfuseResponse& v, std::ostringstream& out) {
                out << "test_efuse success=" << static_cast<unsigned>(v.success)
                    << " component_type=" << static_cast<unsigned>(v.component_type)
                    << " enable=" << static_cast<unsigned>(v.enable)
                    << " error_code=" << static_cast<unsigned>(v.error_code);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_ESTOP_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestEstopResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestEstopResponse& v, std::ostringstream& out) {
                out << "test_estop success=" << static_cast<unsigned>(v.success)
                    << " error_code=" << static_cast<unsigned>(v.error_code);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_IO_EXPANDER_READ_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestIoExpanderReadResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestIoExpanderReadResponse& v, std::ostringstream& out) {
                out << "test_io_read success=" << static_cast<unsigned>(v.success)
                    << " input_index=" << static_cast<unsigned>(v.input_index)
                    << " value_read=" << static_cast<unsigned>(v.value_read)
                    << " error_code=" << static_cast<unsigned>(v.error_code);
              })) {
        return oss.str();
      }
      break;
    case bluelink::PayloadTypeIds::TEST_IO_EXPANDER_WRITE_RESPONSE:
      if (tryFormatTelemetryPayload<bluelink::TelemetryPayload::TestIoExpanderWriteResponse>(
              payload, payload_len, oss,
              [](const bluelink::TelemetryPayload::TestIoExpanderWriteResponse& v, std::ostringstream& out) {
                out << "test_io_write success=" << static_cast<unsigned>(v.success)
                    << " output_index=" << static_cast<unsigned>(v.output_index)
                    << " value_written=" << static_cast<unsigned>(v.value_written)
                    << " error_code=" << static_cast<unsigned>(v.error_code);
              })) {
        return oss.str();
      }
      break;
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
    case bluelink::PayloadTypeIds::POWER_TELEMETRY:
      return "POWER_TELEMETRY";
    case bluelink::PayloadTypeIds::DRIVE_CONTROL_TELEMETRY:
      return "DRIVE_CONTROL_TELEMETRY";
    case bluelink::PayloadTypeIds::PTO_TELEMETRY:
      return "PTO_TELEMETRY";
    case bluelink::PayloadTypeIds::FOUR_WHEEL_DRIVE_TELEMETRY:
      return "FOUR_WHEEL_DRIVE_TELEMETRY";
    case bluelink::PayloadTypeIds::THREE_POINT_HITCH_TELEMETRY:
      return "THREE_POINT_HITCH_TELEMETRY";
    case bluelink::PayloadTypeIds::SPRAYERS_TELEMETRY:
      return "SPRAYERS_TELEMETRY";
    case bluelink::PayloadTypeIds::RAW_SENSORS_TELEMETRY:
      return "RAW_SENSORS_TELEMETRY";
    case bluelink::PayloadTypeIds::LLC_SYSTEM_CONFIG_TELEMETRY:
      return "LLC_SYSTEM_CONFIG_TELEMETRY";
    case bluelink::PayloadTypeIds::REVERSER_TELEMETRY:
      return "REVERSER_TELEMETRY";
    case bluelink::PayloadTypeIds::STROBE_TELEMETRY:
      return "STROBE_TELEMETRY";
    case bluelink::PayloadTypeIds::SEAT_TELEMETRY:
      return "SEAT_TELEMETRY";
    case bluelink::PayloadTypeIds::REVERSER_ANALOG_CHANNEL_TELEMETRY:
      return "REVERSER_ANALOG_CHANNEL_TELEMETRY";
    case bluelink::PayloadTypeIds::LLC_STATE_TELEMETRY:
      return "LLC_STATE_TELEMETRY";
    case bluelink::PayloadTypeIds::TRANSM_OUT_SPD_TELEMETRY:
      return "TRANSM_OUT_SPD_TELEMETRY";
    case bluelink::PayloadTypeIds::PPI_PP_TELEMETRY:
      return "PPI_PP_TELEMETRY";
    case bluelink::PayloadTypeIds::TEST_HEARTBEAT_RESPONSE:
      return "TEST_HEARTBEAT_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_FW_VERSION_RESPONSE:
      return "TEST_FW_VERSION_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_DIGITAL_WRITE_RESPONSE:
      return "TEST_DIGITAL_WRITE_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_DIGITAL_READ_RESPONSE:
      return "TEST_DIGITAL_READ_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_ANALOG_READ_RESPONSE:
      return "TEST_ANALOG_READ_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_EFUSE_RESPONSE:
      return "TEST_EFUSE_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_ESTOP_RESPONSE:
      return "TEST_ESTOP_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_IO_EXPANDER_READ_RESPONSE:
      return "TEST_IO_EXPANDER_READ_RESPONSE";
    case bluelink::PayloadTypeIds::TEST_IO_EXPANDER_WRITE_RESPONSE:
      return "TEST_IO_EXPANDER_WRITE_RESPONSE";
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
