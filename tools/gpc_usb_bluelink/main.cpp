#include <unistd.h>

#include <poll.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "bluelink_communication_handler.hpp"
#include "bluelink_packet_log.hpp"
#include "distributed_can_id.hpp"
#include "concrete_bluelink_callbacks.hpp"
#include "serial.h"

namespace {

constexpr const char* kDefaultPort = "/dev/ttyACM0";
constexpr int kDefaultBaud = 115200;
constexpr int kSerialTimeoutMs = 700;

serial::Serial g_serial;
uint8_t g_rx_buffer[kCommBufferSize]{};

void delayMs(unsigned ms) { usleep(ms * 1000); }

bool parseInbound(bluelink::PayloadTypeIds /*payload_type*/, const uint8_t* /*buffer*/) { return true; }

void printUsage(const char* prog) {
  std::cerr
      << "Usage: " << prog << " [options]\n"
      << "  Send a bluelink packet over USB CDC serial to GPC (or any destination).\n\n"
      << "Options:\n"
      << "  -p, --port PATH           Serial port (default " << kDefaultPort << ")\n"
      << "  -d, --dst ID              Destination component id, decimal or 0x hex (default 17 / GPC)\n"
      << "  -s, --src ID              Source component id (default 2 / HLC)\n"
      << "  -t, --payload-type ID     PayloadTypeIds numeric value, decimal or 0x hex (required)\n"
      << "  -P, --payload HEX         Payload bytes as hex (optional; zero-filled to SDK size if omitted)\n"
      << "  -q, --qos none|ack        QoS (default none)\n"
      << "  -r, --retries N           ACK retries when qos=ack (default 5)\n"
      << "  --timeout-ms MS           ACK wait timeout (default 2000)\n"
      << "  --log                     Listen and print incoming bluelink packets\n"
      << "  -h, --help                Show this help\n\n"
      << "Example:\n"
      << "  " << prog << " -p /dev/ttyACM0 -t 99 -P 010501 -q ack\n"
      << "  " << prog << " -t 7 -d 17 -q none\n"
      << "  " << prog << " --log -p /dev/ttyACM0\n";
}

bool parseUint(const std::string& text, uint32_t& out) {
  if (text.empty()) {
    return false;
  }
  char* end = nullptr;
  const int base = (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) ? 16 : 10;
  const unsigned long value = std::strtoul(text.c_str(), &end, base);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parsePayloadType(const std::string& text, bluelink::PayloadTypeIds& out) {
  uint32_t numeric = 0;
  if (!parseUint(text, numeric)) {
    return false;
  }
  out = static_cast<bluelink::PayloadTypeIds>(numeric);
  return true;
}

bool parseHexPayload(const std::string& text, std::vector<uint8_t>& out) {
  out.clear();
  std::string hex;
  hex.reserve(text.size());
  for (char c : text) {
    if (c == ' ' || c == ':' || c == ',' || c == '-') {
      continue;
    }
    hex.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  if (hex.size() % 2 != 0) {
    return false;
  }
  for (size_t i = 0; i < hex.size(); i += 2) {
    const std::string byte_str = hex.substr(i, 2);
    char* end = nullptr;
    const unsigned long value = std::strtoul(byte_str.c_str(), &end, 16);
    if (end != byte_str.c_str() + 2) {
      return false;
    }
    out.push_back(static_cast<uint8_t>(value));
  }
  return true;
}

bool parseQos(const std::string& text, bluelink::QosTypes& out) {
  if (text == "none" || text == "NONE" || text == "0") {
    out = bluelink::QosTypes::NONE;
    return true;
  }
  if (text == "ack" || text == "ACK" || text == "require_ack" || text == "REQUIRE_ACK") {
    out = bluelink::QosTypes::REQUIRE_ACK;
    return true;
  }
  return false;
}

bool openSerial(const std::string& port) {
  g_serial.setPort(port);
  g_serial.setBaudrate(kDefaultBaud);
  g_serial.setParity(serial::parity_none);
  serial::Timeout timeout = serial::Timeout::simpleTimeout(kSerialTimeoutMs);
  g_serial.setTimeout(timeout);
  try {
    g_serial.open();
    g_serial.close();
  } catch (const std::exception& e) {
    std::cerr << "Cannot open port " << port << ": " << e.what() << '\n';
    return false;
  }
  return true;
}

struct CliOptions {
  std::string port = kDefaultPort;
  uint8_t dst = bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER;
  uint8_t src = bluelink::HLC_ADDRESS;
  bluelink::PayloadTypeIds payload_type = bluelink::PayloadTypeIds::UNKNOWN;
  std::vector<uint8_t> payload{};
  bluelink::QosTypes qos = bluelink::QosTypes::NONE;
  uint8_t retries = 5;
  unsigned timeout_ms = 2000;
  bool payload_type_set = false;
  bool log_mode = false;
  bool quiet = false;
};

bool parseArgs(int argc, char* argv[], CliOptions& opts) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto needValue = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << '\n';
        return {};
      }
      return argv[++i];
    };

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      std::exit(0);
    } else if (arg == "-p" || arg == "--port") {
      opts.port = needValue(arg.c_str());
      if (opts.port.empty()) {
        return false;
      }
    } else if (arg == "-d" || arg == "--dst") {
      uint32_t v = 0;
      if (!parseUint(needValue(arg.c_str()), v)) {
        return false;
      }
      opts.dst = static_cast<uint8_t>(v);
    } else if (arg == "-s" || arg == "--src") {
      uint32_t v = 0;
      if (!parseUint(needValue(arg.c_str()), v)) {
        return false;
      }
      opts.src = static_cast<uint8_t>(v);
    } else if (arg == "-t" || arg == "--payload-type") {
      if (!parsePayloadType(needValue(arg.c_str()), opts.payload_type)) {
        std::cerr << "Invalid payload type\n";
        return false;
      }
      opts.payload_type_set = true;
    } else if (arg == "-P" || arg == "--payload") {
      if (!parseHexPayload(needValue(arg.c_str()), opts.payload)) {
        std::cerr << "Invalid payload hex\n";
        return false;
      }
    } else if (arg == "-q" || arg == "--qos") {
      if (!parseQos(needValue(arg.c_str()), opts.qos)) {
        std::cerr << "Invalid qos (use none or ack)\n";
        return false;
      }
    } else if (arg == "-r" || arg == "--retries") {
      uint32_t v = 0;
      if (!parseUint(needValue(arg.c_str()), v)) {
        return false;
      }
      opts.retries = static_cast<uint8_t>(v);
    } else if (arg == "--timeout-ms") {
      uint32_t v = 0;
      if (!parseUint(needValue(arg.c_str()), v)) {
        return false;
      }
      opts.timeout_ms = v;
    } else if (arg == "--log") {
      opts.log_mode = true;
    } else if (!arg.empty() && arg[0] != '-') {
      opts.port = arg;
    } else {
      std::cerr << "Unknown option: " << arg << '\n';
      return false;
    }
  }
  if (opts.log_mode) {
    return true;
  }
  return opts.payload_type_set;
}

size_t wireSizeForPayloadType(bluelink::PayloadTypeIds payload_type) {
  bluelink::Header header{};
  header.payload_type = payload_type;
  return sizeof(bluelink::Prefix) + sizeof(bluelink::Header) +
         bluelink::Serializer::GetSizeOfPayload(header) + sizeof(bluelink::Suffix);
}

template <typename T>
void fillPayload(bluelink::Packet<T>& packet, const std::vector<uint8_t>& payload) {
  std::memset(&packet.payload_data, 0, sizeof(T));
  if (!payload.empty()) {
    const size_t copy_size = std::min(payload.size(), sizeof(T));
    std::memcpy(&packet.payload_data, payload.data(), copy_size);
  }
}

template <typename T>
bool sendAndWait(bluelink::BluelinkCommunicationHandler& bluelink, bluelink::Packet<T>& packet,
                 const CliOptions& opts) {
  int tick = 0;
  const int wire_size = (opts.qos == bluelink::QosTypes::REQUIRE_ACK)
                            ? bluelink.writeMessageNow(packet, opts.retries)
                            : bluelink.writeMessageNow(packet);
  if (wire_size <= 0) {
    if (not opts.quiet) {
      std::cerr << "Failed to send packet\n";
    }
    return false;
  }

  if (opts.qos == bluelink::QosTypes::REQUIRE_ACK) {
    delayMs(100);
    bluelink.processReceivedData(g_rx_buffer);

    unsigned elapsed = 100;
    while (bluelink.getLeftRetries(packet) > 0 && elapsed < opts.timeout_ms) {
      bluelink.resendWaitingForAckMsgsLoop(static_cast<uint32_t>(tick++));
      delayMs(100);
      bluelink.processReceivedData(g_rx_buffer);
      elapsed += 100;
    }

    if (bluelink.getLeftRetries(packet) != AckVector::MESSAGE_RECEIVED) {
      if (not opts.quiet) {
        if (bluelink.getLeftRetries(packet) == AckVector::MESSAGE_NACK) {
          std::cerr << "NACK received\n";
        } else {
          std::cerr << "ACK timeout after " << opts.timeout_ms << " ms\n";
        }
      }
      return false;
    }
    if (not opts.quiet) {
      std::cout << "ACK received\n";
    }
  }

  if (not opts.quiet) {
    std::cout << "Sent " << wireSizeForPayloadType(opts.payload_type) << " bytes to " << opts.port
              << " (dst=" << static_cast<int>(opts.dst) << " type=" << static_cast<int>(opts.payload_type)
              << " payload=" << bluelink::Serializer::GetSizeOfPayload(packet.header) << ")\n";
  }
  return true;
}

#define BL_SEND_CASE(payload_id, payload_type)                         \
  case bluelink::PayloadTypeIds::payload_id: {                         \
    bluelink::Packet<payload_type> packet(bluelink::PayloadTypeIds::payload_id, opts.dst, opts.qos); \
    fillPayload(packet, opts.payload);                                 \
    return sendAndWait(bluelink, packet, opts);                        \
  }

bool dispatchSend(bluelink::BluelinkCommunicationHandler& bluelink, const CliOptions& opts) {
  switch (opts.payload_type) {
    BL_SEND_CASE(KEEP_ALIVE, bluelink::ConnectivityPayload::KeepAlive)
    BL_SEND_CASE(ACK_PACKET_RECEIVED, bluelink::ConnectivityPayload::AckPacketReceived)
    BL_SEND_CASE(STEERING_CONTINUOUS_COMMAND, bluelink::CommandsPayload::SteeringContinuousCommand)
    BL_SEND_CASE(THROTTLE_CONTINUOUS_COMMAND, bluelink::CommandsPayload::ThrottleContinuousCommand)
    BL_SEND_CASE(DRIVE_COMMAND, bluelink::CommandsPayload::DriveCommand)
    BL_SEND_CASE(PTO_COMMAND, bluelink::CommandsPayload::PtoCommand)
    BL_SEND_CASE(SPRAYERS_COMMAND, bluelink::CommandsPayload::SprayersCommand)
    BL_SEND_CASE(CALIBRATE_COMMAND, bluelink::CommandsPayload::CalibrateCommand)
    BL_SEND_CASE(ENGINE_START_COMMAND, bluelink::CommandsPayload::EngineStartCommand)
    BL_SEND_CASE(THREE_POINT_HITCH_COMMAND, bluelink::CommandsPayload::ThreePointHitchCommand)
    BL_SEND_CASE(SAFETY_LOCK_COMMAND, bluelink::CommandsPayload::SafetyLockCommand)
    BL_SEND_CASE(HORN_CONTINUOUS_COMMAND, bluelink::CommandsPayload::HornContinuousCommand)
    BL_SEND_CASE(STROBE_COMMAND, bluelink::CommandsPayload::StrobeCommand)
    BL_SEND_CASE(REVERSER_COMMAND, bluelink::CommandsPayload::ReverserCommand)
    BL_SEND_CASE(FOUR_WHEEL_DRIVE_COMMAND, bluelink::CommandsPayload::FourWheelDriveCommand)
    BL_SEND_CASE(STEERING_TELEMETRY, bluelink::TelemetryPayload::SteeringTelemetry)
    BL_SEND_CASE(STEERING_PHYSICAL_SETTINGS_TELEMETRY, bluelink::TelemetryPayload::SteeringPhysicalSettingsTelemetry)
    BL_SEND_CASE(BRAKES_TELEMETRY, bluelink::TelemetryPayload::BrakesTelemetry)
    BL_SEND_CASE(ENGINE_TELEMETRY, bluelink::TelemetryPayload::EngineTelemetry)
    BL_SEND_CASE(DRIVE_CONTROL_TELEMETRY, bluelink::TelemetryPayload::DriveControlTelemetry)
    BL_SEND_CASE(PTO_TELEMETRY, bluelink::TelemetryPayload::PtoTelemetry)
    BL_SEND_CASE(THREE_POINT_HITCH_TELEMETRY, bluelink::TelemetryPayload::ThreePointHitchTelemetry)
    BL_SEND_CASE(SPRAYERS_TELEMETRY, bluelink::TelemetryPayload::SprayersTelemetry)
    BL_SEND_CASE(RAW_SENSORS_TELEMETRY, bluelink::TelemetryPayload::RawSensorTelemetries)
    BL_SEND_CASE(LLC_SYSTEM_STATUS_TELEMETRY, bluelink::TelemetryPayload::LlcSystemStatusTelemetry)
    BL_SEND_CASE(LLC_SYSTEM_CONFIG_TELEMETRY, bluelink::TelemetryPayload::LlcSystemConfigTelemetry)
    BL_SEND_CASE(CALIBRATION_TELEMETRY, bluelink::TelemetryPayload::CalibrationTelemetryData)
    BL_SEND_CASE(HORN_TELEMETRY, bluelink::TelemetryPayload::HornTelemetry)
    BL_SEND_CASE(FOUR_WHEEL_DRIVE_TELEMETRY, bluelink::TelemetryPayload::FourWheelDriveTelemetry)
    BL_SEND_CASE(RESET_COMMAND, bluelink::CommandsPayload::ResetCommand)
    BL_SEND_CASE(CONTROL_METRICS, bluelink::TelemetryPayload::ControlMetricsTelemetryData)
    BL_SEND_CASE(AUTOTUNE_TELEMETRY, bluelink::TelemetryPayload::AutotuneTelemetryData)
    BL_SEND_CASE(SCHEDULE_COMMAND, bluelink::CommandsPayload::ScheduleCommand)
    BL_SEND_CASE(BRAKES_CONTINUOUS_COMMAND, bluelink::CommandsPayload::BrakesContinuousCommand)
    BL_SEND_CASE(REVERSER_TELEMETRY, bluelink::TelemetryPayload::ReverserTelemetry)
    BL_SEND_CASE(STROBE_TELEMETRY, bluelink::TelemetryPayload::StrobeTelemetry)
    BL_SEND_CASE(SEAT_TELEMETRY, bluelink::TelemetryPayload::SeatTelemetry)
    BL_SEND_CASE(LEDBAR_COMMAND, bluelink::CommandsPayload::LedbarCommand)
    BL_SEND_CASE(DRIVER_STATE_COMMAND, bluelink::CommandsPayload::DriverStateCommand)
    BL_SEND_CASE(CONTROL_COMMAND, bluelink::CommandsPayload::ControlCommand)
    BL_SEND_CASE(CENTER_OFFSET_COMMAND, bluelink::CommandsPayload::CenterOffsetCommand)
    BL_SEND_CASE(AUX_KEEP_ALIVE_COMMAND, bluelink::CommandsPayload::AuxKeepAliveCommand)
    BL_SEND_CASE(AUX_KEEP_ALIVE_TELEMETRY, bluelink::TelemetryPayload::AuxKeepAliveTelemetry)
    BL_SEND_CASE(NACK_PACKET_RECEIVED, bluelink::ConnectivityPayload::AckPacketReceived)
    BL_SEND_CASE(CONTROLLER_META_DATA_TELEMETRY, bluelink::TelemetryPayload::ControllerMetaData)
    BL_SEND_CASE(POWER_PANEL_SET_PARAMS_COMMAND, bluelink::CommandsPayload::PowerPanelSetParamsCommand)
    BL_SEND_CASE(POWER_PANEL_SET_MODE_COMMAND, bluelink::CommandsPayload::PowerPanelSetModeCommand)
    BL_SEND_CASE(POWER_PANEL_COMPONENT_CONTROL_COMMAND, bluelink::CommandsPayload::PowerPanelComponentControlCommand)
    BL_SEND_CASE(POWER_PANEL_ESTOP_COMMAND, bluelink::CommandsPayload::PowerPanelEstopCommand)
    BL_SEND_CASE(LOG, bluelink::ConnectivityPayload::Log)
    BL_SEND_CASE(POWER_PANEL_HIGH_FREQ_TELEMETRY, bluelink::TelemetryPayload::PowerPanelHighFreqTelemetry)
    BL_SEND_CASE(POWER_PANEL_LOW_FREQ_TELEMETRY, bluelink::TelemetryPayload::PowerPanelLowFreqTelemetry)
    BL_SEND_CASE(PPI_PP_TELEMETRY, bluelink::TelemetryPayload::PpiPpTelemetry)
    BL_SEND_CASE(REVERSER_ANALOG_CHANNEL_TELEMETRY, bluelink::TelemetryPayload::ReverserAnalogChannelTelemetry)
    BL_SEND_CASE(ENGINE_TORQ_MODE_TELEMETRY, bluelink::TelemetryPayload::EngTorqModeTelemetry)
    BL_SEND_CASE(SEAT_COMMAND, bluelink::CommandsPayload::SeatCommand)
    BL_SEND_CASE(ENGAGE_AUTONOMOUS_COMMAND, bluelink::CommandsPayload::EngageAutonomousCommand)
    BL_SEND_CASE(LLC_STATE_TELEMETRY, bluelink::TelemetryPayload::LlcStateTelemetry)
    BL_SEND_CASE(CONTROLLER_STATE_TELEMETRY, bluelink::TelemetryPayload::ControllerStateTelemetry)
    BL_SEND_CASE(POWER_TELEMETRY, bluelink::TelemetryPayload::PowerTelemetry)
    BL_SEND_CASE(LLC_CONTROLLER_TELEMETRIES, bluelink::TelemetryPayload::LlcControllerTelemetries)
    BL_SEND_CASE(LLC_HIGH_FREQ_SYSTEM_TELEMETRIES, bluelink::TelemetryPayload::LlcHighFreqSystemTelemetries)
    BL_SEND_CASE(LLC_LOW_FREQ_SYSTEM_TELEMETRIES, bluelink::TelemetryPayload::LlcLowFreqSystemTelemetries)
    BL_SEND_CASE(TRANSM_OUT_SPD_TELEMETRY, bluelink::TelemetryPayload::TransmOutSpdTelemetry)
    BL_SEND_CASE(BLUELINK_VERSION_TELEMETRY, bluelink::TelemetryPayload::BluelinkVersionTelemetry)
    BL_SEND_CASE(TRACTOR_DIAGNOSTIC_TELEMETRY, bluelink::TelemetryPayload::TractorDiagnosticTelemetry)
    BL_SEND_CASE(POWER_COMMAND, bluelink::CommandsPayload::ControlCommand)
    BL_SEND_CASE(MICRO_DIGITAL_GPIO_WRITE_COMMAND, bluelink::MicroCommandsPayload::MicroDigitalGpioWriteCommand)
    BL_SEND_CASE(MICRO_DIGITAL_GPIO_READ_COMMAND, bluelink::MicroCommandsPayload::MicroDigitalGpioReadCommand)
    BL_SEND_CASE(MICRO_ADC_READ_COMMAND, bluelink::MicroCommandsPayload::MicroAdcReadCommand)
    BL_SEND_CASE(MICRO_DAC_WRITE_COMMAND, bluelink::MicroCommandsPayload::MicroDacWriteCommand)
    BL_SEND_CASE(MICRO_PWM_SET_COMMAND, bluelink::MicroCommandsPayload::MicroPwmSetCommand)
    BL_SEND_CASE(MICRO_DELAY_MS_COMMAND, bluelink::MicroCommandsPayload::MicroDelayMsCommand)
    BL_SEND_CASE(MICRO_CAN_TRANSMIT_COMMAND, bluelink::MicroCommandsPayload::MicroCanTransmitCommand)
    BL_SEND_CASE(MICRO_UART_TRANSMIT_COMMAND, bluelink::MicroCommandsPayload::MicroUartTransmitCommand)
    BL_SEND_CASE(MICRO_SPI_TRANSFER_COMMAND, bluelink::MicroCommandsPayload::MicroSpiTransferCommand)
    BL_SEND_CASE(MICRO_I2C_WRITE_COMMAND, bluelink::MicroCommandsPayload::MicroI2cWriteCommand)
    BL_SEND_CASE(PROGRAMMING_COMMAND, bluelink::Pattern)
    BL_SEND_CASE(PROGRAMMING_REQUEST, bluelink::Pattern)
    BL_SEND_CASE(STOP_PROGRAMMING_COMMAND, bluelink::Pattern)
    default: {
      bluelink::Packet<bluelink::Pattern> packet(opts.payload_type, opts.dst, opts.qos);
      fillPayload(packet, opts.payload);
      return sendAndWait(bluelink, packet, opts);
    }
  }
}

#undef BL_SEND_CASE

bool prepareSendOptions(CliOptions& opts, std::string& error) {
  bluelink::Header size_probe{};
  size_probe.payload_type = opts.payload_type;
  const size_t expected_payload_size = bluelink::Serializer::GetSizeOfPayload(size_probe);
  if (opts.payload.empty()) {
    opts.payload.assign(expected_payload_size, 0);
  } else if (opts.payload.size() != expected_payload_size) {
    error = "payload size mismatch";
    return false;
  }
  return true;
}

void reportSendResult(bool ok, const std::string& error = {}) {
  if (ok) {
    std::cerr << ">>SENT ok\n" << std::flush;
  } else {
    std::cerr << ">>SENT err " << (error.empty() ? "send failed" : error) << '\n' << std::flush;
  }
}

bool handleStdinSend(const std::string& line, bluelink::BluelinkCommunicationHandler& bluelink) {
  if (line.rfind("SEND ", 0) != 0) {
    return false;
  }

  std::istringstream iss(line.substr(5));
  uint32_t type_id = 0;
  uint32_t dst = 0;
  uint32_t src = bluelink::HLC_ADDRESS;
  uint32_t qos_val = 0;
  std::string hex;
  if (not(iss >> type_id >> dst >> src >> qos_val >> hex)) {
    reportSendResult(false, "bad SEND command");
    return true;
  }

  CliOptions opts;
  opts.dst = static_cast<uint8_t>(dst);
  opts.src = static_cast<uint8_t>(src);
  opts.payload_type = static_cast<bluelink::PayloadTypeIds>(type_id);
  opts.qos = qos_val != 0U ? bluelink::QosTypes::REQUIRE_ACK : bluelink::QosTypes::NONE;
  opts.quiet = true;
  if (not parseHexPayload(hex, opts.payload)) {
    reportSendResult(false, "invalid payload hex");
    return true;
  }

  std::string error;
  if (not prepareSendOptions(opts, error)) {
    reportSendResult(false, error);
    return true;
  }

  reportSendResult(dispatchSend(bluelink, opts));
  return true;
}

void pollStdinSends(bluelink::BluelinkCommunicationHandler& bluelink) {
  struct pollfd pfd {};
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;
  if (poll(&pfd, 1, 0) <= 0) {
    return;
  }

  std::string line;
  if (not std::getline(std::cin, line)) {
    return;
  }
  handleStdinSend(line, bluelink);
}

bool runLogMode(const CliOptions& opts) {
  g_serial.setPort(opts.port);
  g_serial.setBaudrate(kDefaultBaud);
  g_serial.setParity(serial::parity_none);
  serial::Timeout timeout = serial::Timeout::simpleTimeout(kSerialTimeoutMs);
  g_serial.setTimeout(timeout);
  try {
    g_serial.open();
  } catch (const std::exception& e) {
    std::cerr << "Cannot open port " << opts.port << ": " << e.what() << '\n';
    return false;
  }

  std::cout << "Listening on " << opts.port << " (bluelink log)\n" << std::flush;

  ConcreteBluelinkCallbacks callbacks(&g_serial, parseInbound, false);
  bluelink::BluelinkCommunicationHandler bluelink(opts.src, &callbacks);
  BluelinkPacketLog parser;
  uint8_t chunk[kCommBufferSize]{};
  while (true) {
    pollStdinSends(bluelink);

    const size_t available = g_serial.available();
    if (available > 0) {
      const size_t to_read = std::min(available, sizeof(chunk));
      const size_t read_count = g_serial.read(chunk, to_read);
      if (read_count > 0) {
        parser.feed(chunk, read_count);
      }
    } else {
      delayMs(10);
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  CliOptions opts;
  if (not parseArgs(argc, argv, opts)) {
    printUsage(argv[0]);
    return 1;
  }

  if (opts.log_mode) {
    return runLogMode(opts) ? 0 : 1;
  }

  if (not openSerial(opts.port)) {
    return 1;
  }

  bluelink::Header size_probe{};
  size_probe.payload_type = opts.payload_type;
  std::string error;
  if (not prepareSendOptions(opts, error)) {
    std::cerr << "Payload size mismatch: " << error << '\n';
    return 1;
  }

  ConcreteBluelinkCallbacks callbacks(&g_serial, parseInbound);
  bluelink::BluelinkCommunicationHandler bluelink(opts.src, &callbacks);

  if (not dispatchSend(bluelink, opts)) {
    g_serial.close();
    return 1;
  }

  g_serial.close();
  return 0;
}
