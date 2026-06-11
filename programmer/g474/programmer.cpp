#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#define delay(milis) usleep(milis * 1000)

#include "../../3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include/bluelink_messages.hpp"
#include "../../3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include/distributed_can_id.hpp"
#include "../../interfaces/interfaces_g474/MemoryMap/memory_map.hpp"
#include "../../versions.hpp"
#include "bluelink_transport.hpp"
#include "can_transport.hpp"
#include "usb_transport.hpp"

const std::string DEFAULT_CAN_INTERFACE = "can1";
const std::string DEFAULT_USB_PORT = "/dev/ttyACM0";
const std::string DEFAULT_APP_BIN = "../../application_projects/application_g474/Debug/application_g474.bin";
const std::string DEFAULT_CONFIG_BIN = "../../config_projects/config_g474/Debug/config_g474.bin";
const bluelink::ComponentId DEFAULT_FLASHED_CONTROLLER =
    bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER;
const int START_ADDRESS_APPLICATION = FLASH_APPLICATION_ADDRESS;
const int START_ADDRESS_CONFIG = FLASH_CONFIG_ADDRESS;

int g_start_address = START_ADDRESS_APPLICATION;
uint64_t g_file_size = 1;
bluelink::ComponentId g_flashed_controller = DEFAULT_FLASHED_CONTROLLER;

enum class FlashTarget {
  APPLICATION,
  CONFIG,
  CUSTOMIZED_CONFIG
};
FlashTarget g_flash_target = FlashTarget::APPLICATION;

enum class TransportType {
  CAN,
  USB
};

const std::map<std::string, bluelink::ComponentId> COMPONENT_ID_MAP = {
    {"gpc", bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER},
};

const std::map<std::string, FlashTarget> FLASH_TARGET_MAP = {
    {"app", FlashTarget::APPLICATION},
    {"config", FlashTarget::CONFIG},
    {"customized_config", FlashTarget::CUSTOMIZED_CONFIG},
};

const std::map<uint8_t, std::string> CONFIG_TYPE_TO_BIN_MAP = {
    {static_cast<uint8_t>(1), DEFAULT_CONFIG_BIN},
};

struct ProgrammerOptions {
  TransportType transport = TransportType::CAN;
  std::string can_interface = DEFAULT_CAN_INTERFACE;
  std::string usb_port = DEFAULT_USB_PORT;
  std::string controller_type = "gpc";
  std::string flash_target = "app";
  std::string bin_file;
};

BluelinkTransport* g_transport = nullptr;

bool isFileValid(const std::string& file_name) {
  std::ifstream file(file_name, std::ios::binary);

  if (!file) {
    std::cout << "Error: Failed to open file: " << file_name << std::endl;
    return false;
  }

  file.seekg(0, std::ios::end);
  g_file_size = file.tellg();

  const uint32_t max_size = (g_flash_target == FlashTarget::APPLICATION) ? FLASH_APPLICATION_BYTES_SIZE
                                                                         : FLASH_CONFIG_BYTES_SIZE;

  if (g_file_size > max_size) {
    std::cout << "Error: bin file is too big (" << g_file_size << " bytes) for the selected target (max "
              << max_size << " bytes)" << std::endl;
    return false;
  }

  return true;
}

bool writeFlashData(uint32_t address, uint32_t data) {
  bluelink::CommandsPayload::ProgrammingCommand prog_cmd{};
  prog_cmd.programming_command_union.programming_command_data.programming_address = address;
  prog_cmd.programming_command_union.programming_command_data.programming_data = data;

  bluelink::CommandsPayload::ProgrammingCommand data_received{};
  const int tries = 100;

  for (int i = 0; i < tries; ++i) {
    g_transport->sendProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, prog_cmd);
    if (g_transport->receiveProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, data_received,
                                               50)) {
      const uint32_t received_addr =
          data_received.programming_command_union.programming_command_data.programming_address;
      if (received_addr == address) {
        return true;
      }
    }
  }

  return false;
}

bluelink::CommandsPayload::ProgrammingCommand makeProgrammingStartCommand() {
  bluelink::CommandsPayload::ProgrammingCommand prog_cmd{};
  prog_cmd.programming_command_union.programming_command_type =
      bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;
  return prog_cmd;
}

void sendStartProgrammingToApp() {
  g_transport->sendProgrammingCommand(g_flashed_controller, makeProgrammingStartCommand());
}

void sendStartProgramming() {
  sendStartProgrammingToApp();
  g_transport->sendProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, makeProgrammingStartCommand());
}

void sendStartToBootloader() {
  g_transport->sendProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, makeProgrammingStartCommand());
}

bool isProgrammingStartAck(const bluelink::CommandsPayload::ProgrammingCommand& response) {
  return response.programming_command_union.programming_command_type ==
         bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;
}

bool waitForBootloaderProgrammingAck(int timeout_ms) {
  bluelink::CommandsPayload::ProgrammingCommand response{};
  const int step_ms = 100;
  int elapsed = 0;

  while (elapsed < timeout_ms) {
    sendStartToBootloader();
    g_transport->flushOutput();

    if (g_transport->receiveProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, response, step_ms) &&
        isProgrammingStartAck(response)) {
      return true;
    }

    elapsed += step_ms;
  }

  return false;
}

bool isInProgrammingMode() {
  return waitForBootloaderProgrammingAck(15000);
}

bool beginUsbProgrammingSession() {
  std::cout << "Sending programming start to application over USB\n";

  for (int i = 0; i < 3; ++i) {
    sendStartProgrammingToApp();
    g_transport->flushOutput();
    delay(100);
  }

  delay(200);
  std::cout << "Closing USB before bootloader jump\n";
  g_transport->shutdown();
  delay(500);

  std::cout << "Waiting for USB reconnect after reset\n";
  if (!g_transport->reopenAfterReset()) {
    return false;
  }

  std::cout << "USB reconnected, requesting bootloader programming ACK\n";
  return true;
}

void sendReadyFlashing() {
  bluelink::CommandsPayload::ProgrammingCommand prog_cmd{};
  prog_cmd.programming_command_union.programming_command_type =
      (g_flash_target == FlashTarget::APPLICATION)
          ? bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING
          : bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING;

  for (int i = 0; i < 10; ++i) {
    g_transport->sendProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, prog_cmd);
    delay(50);
  }
}

bool sendFinish() {
  bluelink::CommandsPayload::ProgrammingCommand prog_cmd{};
  prog_cmd.programming_command_union.programming_command_type =
      bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE;

  bluelink::CommandsPayload::ProgrammingCommand response{};
  const int tries = 10;

  for (int i = 0; i < tries; ++i) {
    g_transport->sendProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, prog_cmd);
    if (g_transport->receiveProgrammingCommand(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, response, 800)) {
      if (response.programming_command_union.programming_command_type ==
          bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE) {
        return true;
      }
    }
    delay(10);
  }

  return false;
}

void printUsage() {
  std::cout << "Usage:\n";
  std::cout << "  programmer [CAN_INTERFACE] [CONTROLLER_TYPE] [FLASH_TARGET] [BIN_FILE]\n";
  std::cout << "  programmer --transport usb [--port PATH] [CONTROLLER_TYPE] [FLASH_TARGET] [BIN_FILE]\n";
  std::cout << "  programmer --transport can [CAN_INTERFACE] [CONTROLLER_TYPE] [FLASH_TARGET] [BIN_FILE]\n\n";
  std::cout << "  CAN_INTERFACE   - CAN interface name (default: " << DEFAULT_CAN_INTERFACE << ")\n";
  std::cout << "  --port PATH     - USB serial port (default: " << DEFAULT_USB_PORT << ")\n";
  std::cout << "  CONTROLLER_TYPE - Target controller type (default: gpc)\n";
  std::cout << "  FLASH_TARGET    - Flash target area (app, config, or customized_config) (default: app)\n";
  std::cout << "  BIN_FILE        - Path to the binary file (default for app: " << DEFAULT_APP_BIN << ")\n";
  std::cout << "                    (default for config: " << DEFAULT_CONFIG_BIN << ")\n";
  std::cout << "                    (not required when using customized_config target)\n\n";

  std::cout << "Supported controller types:\n";
  for (const auto& entry : COMPONENT_ID_MAP) {
    std::cout << "  " << entry.first << "\n";
  }

  std::cout << "\nFlash target options:\n";
  std::cout << "  app              - Application area (0x" << std::hex << FLASH_APPLICATION_ADDRESS << ", size: 0x"
            << FLASH_APPLICATION_BYTES_SIZE << " bytes)\n";
  std::cout << "  config           - Configuration area (0x" << FLASH_CONFIG_ADDRESS << ", size: 0x"
            << FLASH_CONFIG_BYTES_SIZE << " bytes)\n";
  std::cout << "  customized_config - Automatically select and flash config file based on controller metadata\n"
            << std::dec << std::endl;

  std::cout << "Expected bootloader version (major.minor): " << BOOTLOADER_VERSION_MAJOR << "."
            << BOOTLOADER_VERSION_MINOR << std::endl;
}

void printProgressBar(double percent) {
  const int bar_width = 50;
  std::cout << "\r[";
  const int pos = static_cast<int>(bar_width * percent / 100.0);
  for (int i = 0; i < bar_width; ++i) {
    if (i < pos) {
      std::cout << "=";
    } else if (i == pos) {
      std::cout << ">";
    } else {
      std::cout << " ";
    }
  }
  std::cout << "] " << std::fixed << std::setprecision(2) << percent << "%" << std::flush;
}

void printMetaData(const bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
  std::cout << "Received meta data:\n";
  std::cout << "  Component ID: " << static_cast<int>(meta_data.component_id) << "\n";
  std::cout << "  Bootloader Version: " << static_cast<int>(meta_data.bootloader_version[0]) << "."
            << static_cast<int>(meta_data.bootloader_version[1]) << "\n";
  std::cout << "  App Version: " << static_cast<int>(meta_data.app_version[0]) << "."
            << static_cast<int>(meta_data.app_version[1]) << "\n";
  std::cout << "  Config Version: " << static_cast<int>(meta_data.config_version[0]) << "."
            << static_cast<int>(meta_data.config_version[1]) << "\n";
  std::cout << "  Config Type: " << static_cast<int>(meta_data.config_type) << "\n";
}

bool verifyBootloaderVersion(const uint8_t bootloader_version[2]) {
  if (bootloader_version[0] != BOOTLOADER_VERSION_MAJOR || bootloader_version[1] != BOOTLOADER_VERSION_MINOR) {
    std::cout << "Error: Bootloader version mismatch. Expected " << BOOTLOADER_VERSION_MAJOR << "."
              << BOOTLOADER_VERSION_MINOR << ", got " << static_cast<int>(bootloader_version[0]) << "."
              << static_cast<int>(bootloader_version[1]) << "\n";
    return false;
  }

  std::cout << "Bootloader version verified: " << BOOTLOADER_VERSION_MAJOR << "." << BOOTLOADER_VERSION_MINOR << "\n";
  return true;
}

bool requestControllerMetaData(bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
  if (g_transport->requestMetaData(g_flashed_controller, meta_data)) {
    printMetaData(meta_data);
    return true;
  }

  std::cout << "Failed to receive controller meta data\n";
  return false;
}

bool verifyBootloaderVersionOnDevice(const bluelink::TelemetryPayload::ControllerMetaData* cached_meta) {
  if (cached_meta != nullptr && cached_meta->component_id > 0) {
    return verifyBootloaderVersion(cached_meta->bootloader_version);
  }

  bluelink::TelemetryPayload::ControllerMetaData meta_data{};
  if (g_transport->requestMetaData(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, meta_data)) {
    printMetaData(meta_data);
    return verifyBootloaderVersion(meta_data.bootloader_version);
  }

  if (g_transport->requestMetaData(g_flashed_controller, meta_data)) {
    printMetaData(meta_data);
    return verifyBootloaderVersion(meta_data.bootloader_version);
  }

  std::cout << "Failed to read bootloader version from device\n";
  return false;
}

std::string getConfigFilePath(uint8_t config_type) {
  const auto it = CONFIG_TYPE_TO_BIN_MAP.find(config_type);
  if (it != CONFIG_TYPE_TO_BIN_MAP.end()) {
    return it->second;
  }
  return "";
}

int flashProgram(const std::string& file_name) {
  std::cout << "File size: " << g_file_size << " bytes\n";

  std::ifstream file(file_name, std::ios::binary);
  file.seekg(0, std::ios::beg);

  uint32_t address = g_start_address;
  uint32_t data = 0;

  std::cout << "Start Flashing to address 0x" << std::hex << address << std::dec << "\n";
  while (file.read(reinterpret_cast<char*>(&data), sizeof(data))) {
    const double percent = (static_cast<double>(address - g_start_address) / g_file_size) * 100;
    printProgressBar(percent);
    if (!writeFlashData(address, data)) {
      std::cout << "\nFailed to write address " << std::hex << address << "\n";
      return -1;
    }
    address += sizeof(data);
  }

  const int remaining_bytes = g_file_size % 4;
  if (remaining_bytes > 0) {
    data = 0;
    file.clear();
    file.seekg(-remaining_bytes, std::ios::end);
    file.read(reinterpret_cast<char*>(&data), remaining_bytes);

    const double percent = (static_cast<double>(address - g_start_address) / g_file_size) * 100;
    printProgressBar(percent);

    if (!writeFlashData(address, data)) {
      std::cout << "\nFailed to write final partial address " << std::hex << address << "\n";
      return -1;
    }
  }

  file.close();
  sendFinish();

  printProgressBar(100.0);
  std::cout << "\nBoard flashed successfully\n";
  return 0;
}

void applyControllerType(const std::string& controller_type) {
  const auto it = COMPONENT_ID_MAP.find(controller_type);
  if (it != COMPONENT_ID_MAP.end()) {
    g_flashed_controller = it->second;
    std::cout << "Using controller type: " << controller_type << std::endl;
  } else {
    std::cout << "Unknown controller type: " << controller_type << std::endl;
    std::cout << "Using default controller type: gpc" << std::endl;
  }
}

void applyFlashTarget(const std::string& flash_target) {
  const auto it = FLASH_TARGET_MAP.find(flash_target);
  if (it != FLASH_TARGET_MAP.end()) {
    g_flash_target = it->second;
    g_start_address = (g_flash_target == FlashTarget::APPLICATION) ? START_ADDRESS_APPLICATION : START_ADDRESS_CONFIG;
    std::cout << "Using flash target: " << flash_target << " (Address: 0x" << std::hex << g_start_address << std::dec
              << ")" << std::endl;
  } else {
    std::cout << "Unknown flash target: " << flash_target << std::endl;
    std::cout << "Using default flash target: app" << std::endl;
  }
}

bool parseOptions(int argc, char* argv[], ProgrammerOptions& opts) {
  int positional = 1;

  if (argc > 1 && std::string(argv[1]) == "--transport") {
    if (argc < 3) {
      return false;
    }

    const std::string transport = argv[2];
    if (transport == "usb") {
      opts.transport = TransportType::USB;
    } else if (transport == "can") {
      opts.transport = TransportType::CAN;
    } else {
      return false;
    }

    positional = 3;
    while (positional + 1 < argc) {
      const std::string arg = argv[positional];
      if (arg == "--port") {
        opts.usb_port = argv[positional + 1];
        positional += 2;
        continue;
      }
      break;
    }
  }

  if (opts.transport == TransportType::CAN && positional < argc) {
    opts.can_interface = argv[positional++];
  }

  if (positional < argc) {
    opts.controller_type = argv[positional];
    applyControllerType(argv[positional++]);
  }

  if (positional < argc) {
    opts.flash_target = argv[positional];
    applyFlashTarget(argv[positional++]);
  }

  if (positional < argc && std::string(argv[positional]) != "") {
    opts.bin_file = argv[positional];
  } else if (g_flash_target == FlashTarget::CONFIG) {
    opts.bin_file = DEFAULT_CONFIG_BIN;
  } else if (g_flash_target == FlashTarget::APPLICATION) {
    opts.bin_file = DEFAULT_APP_BIN;
  }

  return true;
}

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      printUsage();
      return 0;
    }
  }

  ProgrammerOptions opts;
  if (!parseOptions(argc, argv, opts)) {
    printUsage();
    return -1;
  }

  std::unique_ptr<BluelinkTransport> transport;
  if (opts.transport == TransportType::USB) {
    std::cout << "Using USB transport on " << opts.usb_port << std::endl;
    transport = std::make_unique<UsbTransport>(opts.usb_port);
  } else {
    std::cout << "Using CAN transport on " << opts.can_interface << std::endl;
    transport = std::make_unique<CanTransport>(opts.can_interface);
  }

  g_transport = transport.get();
  if (!g_transport->init()) {
    std::cout << "Failed to initialize transport\n";
    return -1;
  }

  bluelink::TelemetryPayload::ControllerMetaData controller_meta{};
  if (!requestControllerMetaData(controller_meta)) {
    if (opts.transport == TransportType::CAN) {
      g_transport->reopenAfterReset();
      requestControllerMetaData(controller_meta);
    }
  }

  const uint8_t config_type = controller_meta.config_type;
  std::string file_name = opts.bin_file;

  if (g_flash_target == FlashTarget::CUSTOMIZED_CONFIG) {
    if (config_type == 0) {
      std::cout << "Failed to get config type from controller\n";
      g_transport->shutdown();
      return -4;
    }

    file_name = getConfigFilePath(config_type);
    if (file_name.empty()) {
      std::cout << "No matching config bin file for config type " << static_cast<int>(config_type) << "\n";
      g_transport->shutdown();
      return -5;
    }

    std::cout << "Selected config file: " << file_name << " for config type " << static_cast<int>(config_type)
              << "\n";
  }

  if (!isFileValid(file_name)) {
    std::cout << "Invalid bin file!\n";
    g_transport->shutdown();
    return -3;
  }

  if (!verifyBootloaderVersionOnDevice(&controller_meta)) {
    std::cout << "Bootloader version verification failed\n";
    g_transport->shutdown();
    return -6;
  }

  std::cout << "Send start programming request and wait for ACK\n";

  if (opts.transport == TransportType::USB) {
    if (!beginUsbProgrammingSession()) {
      std::cout << "Failed to reconnect USB after bootloader jump\n";
      g_transport->shutdown();
      return -2;
    }
  } else {
    sendStartProgramming();
    delay(500);
  }

  if (!isInProgrammingMode()) {
    std::cout << "Didn't get ACK from controller, trying again\n";
    if (opts.transport == TransportType::USB) {
      std::cout << "Retrying bootloader programming ACK over USB\n";
    } else {
      sendStartProgramming();
      delay(1000);
    }
    if (!isInProgrammingMode()) {
      std::cout << "Didn't get ACK from controller";
      if (opts.transport == TransportType::CAN) {
        std::cout << ", run sudo bash can_setup.sh and try again";
      }
      std::cout << "\n";
      g_transport->shutdown();
      return -2;
    }
  }

  delay(100);
  sendReadyFlashing();
  delay(800);

  if (flashProgram(file_name)) {
    std::cout << "Can't flash bin file!";
    if (opts.transport == TransportType::CAN) {
      std::cout << " run sudo bash can_setup.sh and try again";
    }
    std::cout << "\n";
    g_transport->shutdown();
    return -3;
  }

  g_transport->shutdown();
  std::cout << "Good bye!\n";
  return 0;
}
