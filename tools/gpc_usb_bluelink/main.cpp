#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "bluelink_communication_handler.hpp"
#include "concrete_bluelink_callbacks.hpp"
#include "serial.h"
#include "bluelink_serializer.hpp"
#include "wire_packet.hpp"

namespace {

constexpr const char* kDefaultPort = "/dev/ttyACM0";
constexpr int kDefaultBaud = 115200;
constexpr int kSerialTimeoutMs = 700;

serial::Serial g_serial;
uint8_t g_rx_buffer[kCommBufferSize]{};

uint32_t g_expected_sequence_id = 0;
bool g_ack_received = false;
bool g_nack_received = false;

void delayMs(unsigned ms) { usleep(ms * 1000); }

bool parseInbound(bluelink::PayloadTypeIds payload_type, const uint8_t* buffer) {
  bluelink::Header header{};
  bluelink::Deserializer::decodeHeader(&header, buffer);

  if (payload_type == bluelink::PayloadTypeIds::ACK_PACKET_RECEIVED &&
      header.packet_sequence_id == g_expected_sequence_id) {
    g_ack_received = true;
  } else if (payload_type == bluelink::PayloadTypeIds::NACK_PACKET_RECEIVED &&
             header.packet_sequence_id == g_expected_sequence_id) {
    g_nack_received = true;
  }
  return true;
}

void printUsage(const char* prog) {
  std::cerr
      << "Usage: " << prog << " [options]\n"
      << "  Send a BlueLink packet over USB CDC serial to GPC (or any destination).\n\n"
      << "Options:\n"
      << "  -p, --port PATH           Serial port (default " << kDefaultPort << ")\n"
      << "  -d, --dst ID              Destination component id, decimal or 0x hex (default 17 / GPC)\n"
      << "  -s, --src ID              Source component id (default 2 / HLC)\n"
      << "  -t, --payload-type ID     PayloadTypeIds value or name (required)\n"
      << "  -P, --payload HEX         Payload bytes as hex (optional)\n"
      << "  -q, --qos none|ack        QoS (default none)\n"
      << "  -r, --retries N           ACK retries when qos=ack (default 5)\n"
      << "  --timeout-ms MS           ACK wait timeout (default 2000)\n"
      << "  -h, --help                Show this help\n\n"
      << "Example:\n"
      << "  " << prog << " -p /dev/ttyACM0 -t MICRO_DIGITAL_GPIO_WRITE_COMMAND -P 010501 -q ack\n"
      << "  " << prog << " -t 7 -P 00000000 -d 17 -q none\n";
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
  if (parseUint(text, numeric)) {
    out = static_cast<bluelink::PayloadTypeIds>(numeric);
    return true;
  }

#define MAP_PAYLOAD(NAME) \
  if (text == #NAME) {    \
    out = bluelink::PayloadTypeIds::NAME; \
    return true;          \
  }

  MAP_PAYLOAD(KEEP_ALIVE)
  MAP_PAYLOAD(RESET_COMMAND)
  MAP_PAYLOAD(PROGRAMMING_COMMAND)
  MAP_PAYLOAD(DRIVE_COMMAND)
  MAP_PAYLOAD(CONTROLLER_META_DATA_TELEMETRY)
  MAP_PAYLOAD(BLUELINK_VERSION_TELEMETRY)
  MAP_PAYLOAD(MICRO_DIGITAL_GPIO_WRITE_COMMAND)
  MAP_PAYLOAD(MICRO_DIGITAL_GPIO_READ_COMMAND)
  MAP_PAYLOAD(MICRO_ADC_READ_COMMAND)
  MAP_PAYLOAD(MICRO_DAC_WRITE_COMMAND)
  MAP_PAYLOAD(MICRO_PWM_SET_COMMAND)
  MAP_PAYLOAD(MICRO_DELAY_MS_COMMAND)
  MAP_PAYLOAD(MICRO_CAN_TRANSMIT_COMMAND)
  MAP_PAYLOAD(MICRO_UART_TRANSMIT_COMMAND)
  MAP_PAYLOAD(MICRO_SPI_TRANSFER_COMMAND)
  MAP_PAYLOAD(MICRO_I2C_WRITE_COMMAND)
#undef MAP_PAYLOAD

  return false;
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
  uint8_t dst = 0x11;  // COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER
  uint8_t src = bluelink::HLC_ADDRESS;
  bluelink::PayloadTypeIds payload_type = bluelink::PayloadTypeIds::UNKNOWN;
  std::vector<uint8_t> payload{};
  bluelink::QosTypes qos = bluelink::QosTypes::NONE;
  uint8_t retries = 5;
  unsigned timeout_ms = 2000;
  bool payload_type_set = false;
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
    } else if (!arg.empty() && arg[0] != '-') {
      opts.port = arg;
    } else {
      std::cerr << "Unknown option: " << arg << '\n';
      return false;
    }
  }
  return opts.payload_type_set;
}

bool waitForAck(bluelink::BluelinkCommunicationHandler& bluelink, ConcreteBluelinkCallbacks& callbacks,
                const uint8_t* wire_buffer, size_t wire_size, uint8_t retries, unsigned timeout_ms) {
  const unsigned step_ms = 50;
  unsigned elapsed = 0;
  uint8_t resend_count = 0;

  while (elapsed < timeout_ms && not g_ack_received && not g_nack_received) {
    bluelink.processReceivedData(g_rx_buffer);
    if (g_ack_received) {
      return true;
    }
    if (g_nack_received) {
      std::cerr << "NACK received\n";
      return false;
    }
    delayMs(step_ms);
    elapsed += step_ms;
    if (resend_count < retries && (elapsed % 200) == 0 && elapsed > 0) {
      callbacks.write(wire_buffer, wire_size);
      ++resend_count;
    }
  }

  std::cerr << "ACK timeout after " << timeout_ms << " ms\n";
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  CliOptions opts;
  if (not parseArgs(argc, argv, opts)) {
    printUsage(argv[0]);
    return 1;
  }

  if (not openSerial(opts.port)) {
    return 1;
  }

  bluelink::Header size_probe{};
  size_probe.payload_type = opts.payload_type;
  const size_t expected_payload_size = bluelink::Serializer::GetSizeOfPayload(size_probe);
  if (opts.payload.empty() && expected_payload_size > 0) {
    opts.payload.assign(expected_payload_size, 0);
  }

  if (opts.payload.size() > gpc_usb_bluelink::kMaxWirePayloadBytes) {
    std::cerr << "Payload too large (" << opts.payload.size() << " bytes)\n";
    return 1;
  }

  ConcreteBluelinkCallbacks callbacks(&g_serial, parseInbound);
  bluelink::BluelinkCommunicationHandler bluelink(opts.src, &callbacks);

  static uint8_t wire_buffer[512]{};
  gpc_usb_bluelink::SendOptions send_opts{};
  send_opts.source_id = opts.src;
  send_opts.destination_id = opts.dst;
  send_opts.payload_type = opts.payload_type;
  send_opts.qos = opts.qos;
  if (opts.qos == bluelink::QosTypes::REQUIRE_ACK) {
    send_opts.packet_sequence_id = 1;
  }
  g_expected_sequence_id = send_opts.packet_sequence_id;

  const size_t wire_size = gpc_usb_bluelink::buildWirePacket(wire_buffer, sizeof(wire_buffer), send_opts,
                                                             opts.payload.data(), opts.payload.size());
  if (wire_size == 0) {
    std::cerr << "Failed to build wire packet\n";
    return 1;
  }

  if (callbacks.write(wire_buffer, wire_size) != wire_size) {
    std::cerr << "Serial write failed\n";
    return 1;
  }

  std::cout << "Sent " << wire_size << " bytes to " << opts.port << " (dst=" << static_cast<int>(opts.dst)
            << " type=" << static_cast<int>(opts.payload_type) << " payload=" << opts.payload.size() << ")\n";

  if (opts.qos == bluelink::QosTypes::REQUIRE_ACK) {
    delayMs(100);
    if (not waitForAck(bluelink, callbacks, wire_buffer, wire_size, opts.retries, opts.timeout_ms)) {
      g_serial.close();
      return 1;
    }
    std::cout << "ACK received\n";
  }

  g_serial.close();
  return 0;
}
