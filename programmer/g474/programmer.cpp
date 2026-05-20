#include <string>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <vector>
#include <map>

#include <unistd.h>
#define delay(milis) usleep(milis*1000)

#include "../../interfaces/interfaces_g474/MemoryMap/memory_map.hpp"
#include "../../versions.hpp"
#include "../../3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include/bluelink_messages.hpp"
#include "../../3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include/distributed_can_id.hpp"

// Default values
const std::string DEFAULT_CAN_INTERFACE = "can1";
const std::string DEFAULT_BIN = "../../application_projects/application_g474/Debug/application_g474.bin";
const bluelink::ComponentId DEFAULT_FLASHED_CONTROLLER =
    bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER;
const int START_ADDRESS_APPLICATION = FLASH_APPLICATION_ADDRESS;
const int START_ADDRESS_CONFIG = FLASH_CONFIG_ADDRESS;
const bluelink::J1939CanIdStruct RX_CAN_ID(
        bluelink::ComponentId::COMPONENT_ID_BOOTLOADER,
        bluelink::ComponentId::COMPONENT_ID_HLC,
        bluelink::PayloadTypeIds::PROGRAMMING_COMMAND
    );
static const bluelink::J1939CanIdStruct TX_CAN_ID(
        bluelink::ComponentId::COMPONENT_ID_HLC,
        bluelink::ComponentId::COMPONENT_ID_BOOTLOADER,
        bluelink::PayloadTypeIds::PROGRAMMING_COMMAND
    );

// Globals
int g_start_address = START_ADDRESS_APPLICATION;
int g_can_socket = -1;
uint64_t g_file_size = 1;
bluelink::ComponentId g_flashed_controller = DEFAULT_FLASHED_CONTROLLER;
std::string g_can_interface = DEFAULT_CAN_INTERFACE;

enum class FlashTarget {
    APPLICATION,
    CONFIG,
    CUSTOMIZED_CONFIG
};
FlashTarget g_flash_target = FlashTarget::APPLICATION;

const std::map<std::string, bluelink::ComponentId> COMPONENT_ID_MAP = {
    {"gpc", bluelink::ComponentId::COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER},
};

const std::map<std::string, FlashTarget> FLASH_TARGET_MAP = {
    {"app", FlashTarget::APPLICATION},
    {"config", FlashTarget::CONFIG},
    {"customized_config", FlashTarget::CUSTOMIZED_CONFIG}
};

const std::map<uint8_t, std::string> CONFIG_TYPE_TO_BIN_MAP = {
    {static_cast<uint8_t>(1), "../../config_projects/config_g474/build/fw-config-g4.bin"},
};

void canFilterByDestConfig(int socket_fd) {
    struct can_filter rfilter[1];

    uint32_t destination_mask = 0xFF << 8;
    uint32_t destination_value = static_cast<uint32_t>(bluelink::ComponentId::COMPONENT_ID_HLC) << 8;

    rfilter[0].can_id = destination_value | CAN_EFF_FLAG;
    rfilter[0].can_mask = destination_mask | CAN_EFF_FLAG;

    setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
}

bool setupCanSocket(const std::string& interface_name) {
    g_can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (g_can_socket < 0) {
        perror("Socket");
        return false;
    }

    struct ifreq ifr;
    std::strcpy(ifr.ifr_name, interface_name.c_str());
    if (ioctl(g_can_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
        close(g_can_socket);
        return false;
    }

    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(g_can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        close(g_can_socket);
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 800000;
    if (setsockopt(g_can_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt timeout");
    }

    return true;
}

bool resetCanInterface(const std::string& interface_name) {
    if (g_can_socket >= 0) {
        close(g_can_socket);
        g_can_socket = -1;
    }

    if (!setupCanSocket(interface_name)) {
        std::cout << "Failed to reset CAN interface\n";
        return false;
    }

    canFilterByDestConfig(g_can_socket);
    std::cout << "CAN interface reset successful\n";
    return true;
}

void sendCanMessage(uint32_t can_id, const uint8_t* data, size_t size) {
    struct can_frame frame;

    size_t data_len = size;
    if (data_len > 8) data_len = 8;

    frame.can_id = can_id | CAN_EFF_FLAG;
    frame.can_dlc = data_len;
    std::memcpy(frame.data, data, data_len);

    write(g_can_socket, &frame, sizeof(frame));
}

bool receiveCanMessage(uint32_t expected_addr, uint8_t* data, size_t size) {
    struct can_frame frame;
    int nbytes = read(g_can_socket, &frame, sizeof(frame));

    if (nbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        perror("Read from CAN");
        return false;
    }

    if (nbytes != sizeof(frame)) {
        std::cerr << "Received incomplete CAN frame\n";
        return false;
    }

    if ((frame.can_id & CAN_EFF_MASK) == (expected_addr & CAN_EFF_MASK)) {
        size_t copy_size = (frame.can_dlc < size) ? frame.can_dlc : size;
        std::memcpy(data, frame.data, copy_size);
        return true;
    }

    return false;
}

void sendProgrammingData(const bluelink::CommandsPayload::ProgrammingCommand& prog_cmd) {
    sendCanMessage(
        CONVERT_CAN_ID_TO_UINT32(TX_CAN_ID),
        reinterpret_cast<const uint8_t*>(&prog_cmd),
        sizeof(prog_cmd)
    );
}

bool isFileValid(const std::string& file_name) {
    std::ifstream file(file_name, std::ios::binary);

    if (!file) {
        std::cout << "Error: Failed to open file: " << file_name << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    g_file_size = file.tellg();

    uint32_t max_size = (g_flash_target == FlashTarget::APPLICATION) ?
                        FLASH_APPLICATION_BYTES_SIZE : FLASH_CONFIG_BYTES_SIZE;

    if (g_file_size > max_size) {
        std::cout << "Error: bin file is too big (" << g_file_size << " bytes) for the selected target (max "
                  << max_size << " bytes)" << std::endl;
        return false;
    }

    return true;
}

bool writeFlashData(uint32_t address, uint32_t data) {
    static bluelink::CommandsPayload::ProgrammingCommand prog_cmd;
    prog_cmd.programming_command_union.programming_command_data.programming_address = address;
    prog_cmd.programming_command_union.programming_command_data.programming_data = data;

    bluelink::CommandsPayload::ProgrammingCommand data_received;
    uint32_t received_addr = 0;
    bool is_success = false;
    const int tries = 100;

    for (int i = 0; i < tries && !is_success; i++) {
        sendProgrammingData(prog_cmd);
        static uint32_t rx_id = CONVERT_CAN_ID_TO_UINT32(RX_CAN_ID);
        if (receiveCanMessage(rx_id, reinterpret_cast<uint8_t*>(&data_received), sizeof(data_received))) {
            received_addr = data_received.programming_command_union.programming_command_data.programming_address;
            is_success = received_addr == address;
        }
    }

    return is_success;
}

bool isInProgrammingMode() {
    static uint32_t rx_id = CONVERT_CAN_ID_TO_UINT32(RX_CAN_ID);

    bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState response =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_ERROR;
    const int tries = 100;
    bool is_success = false;

    for (int i = 0; i < tries && !is_success; i++) {
        if (receiveCanMessage(rx_id, reinterpret_cast<uint8_t*>(&response), sizeof(response))) {
            std::cout << "GOT THE RIGHT ID \n";
            is_success = response == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;
        }

        if (!is_success) {
            delay(10);
        }
    }

    return is_success;
}

void sendStartProgramming() {
  static const bluelink::J1939CanIdStruct can_id(
        bluelink::ComponentId::COMPONENT_ID_HLC,
        g_flashed_controller,
        bluelink::PayloadTypeIds::PROGRAMMING_COMMAND
    );

    bluelink::CommandsPayload::ProgrammingCommand prog_cmd;
    prog_cmd.programming_command_union.programming_command_type =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;

    sendCanMessage(
        CONVERT_CAN_ID_TO_UINT32(can_id),
        reinterpret_cast<const uint8_t*>(&prog_cmd),
        sizeof(prog_cmd)
    );

    sendProgrammingData(prog_cmd);
}

void sendReadyFlashing() {
    bluelink::CommandsPayload::ProgrammingCommand prog_cmd;
    prog_cmd.programming_command_union.programming_command_type = (g_flash_target == FlashTarget::APPLICATION) ?
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING :
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING;

    const int tries = 10;
    for (int i = 0; i < tries; i++) {
        sendProgrammingData(prog_cmd);
        delay(50);
    }
}

bool sendFinish() {
    bluelink::CommandsPayload::ProgrammingCommand prog_cmd;
    prog_cmd.programming_command_union.programming_command_type =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE;

    static uint32_t rx_id = CONVERT_CAN_ID_TO_UINT32(RX_CAN_ID);

    bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState response =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_ERROR;

    bool is_success = false;
    const int tries = 10;

    for (int i = 0; i < tries && !is_success; i++) {
        sendProgrammingData(prog_cmd);
        if (receiveCanMessage(rx_id, reinterpret_cast<uint8_t*>(&response), sizeof(response))) {
            is_success = response == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE;
        }

        if (!is_success) {
            delay(10);
        }
    }

    return is_success;
}

void printUsage() {
    std::cout << "Usage: programmer [CAN_INTERFACE] [CONTROLLER_TYPE] [FLASH_TARGET] [BIN_FILE]\n\n";
    std::cout << "  CAN_INTERFACE   - CAN interface name (default: " << DEFAULT_CAN_INTERFACE << ")\n";
    std::cout << "  CONTROLLER_TYPE - Target controller type (default: gpc)\n";
    std::cout << "  FLASH_TARGET    - Flash target area (app, config, or customized_config) (default: app)\n";
    std::cout << "  BIN_FILE        - Path to the binary file (default: " << DEFAULT_BIN << ")\n";
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

    std::cout << "Expected bootloader version (major.minor): "
              << BOOTLOADER_VERSION_MAJOR << "."
              << BOOTLOADER_VERSION_MINOR << "\n\n";

    std::cout << "Supported config types for customized_config:\n";
    for (const auto& entry : CONFIG_TYPE_TO_BIN_MAP) {
        std::cout << "  " << (int)entry.first << " - " << entry.second << "\n";
    }
    std::cout << std::endl;
}

void printProgressBar(double percent) {
    const int bar_width = 50;
    int filled = static_cast<int>(bar_width * percent / 100.0);

    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) {
            std::cout << "=";
        } else if (i == filled) {
            std::cout << ">";
        } else {
            std::cout << " ";
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(2) << percent << "%" << std::flush;
}

void printMetaData(const bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
    std::cout << "Received meta data:\n";
    std::cout << "  Component ID: " << (int)meta_data.component_id << "\n";
    std::cout << "  Bootloader Version: " << (int)meta_data.bootloader_version[0] << "."
              << (int)meta_data.bootloader_version[1] << "\n";
    std::cout << "  App Version: " << (int)meta_data.app_version[0] << "."
              << (int)meta_data.app_version[1] << "\n";
    std::cout << "  Config Version: " << (int)meta_data.config_version[0] << "."
              << (int)meta_data.config_version[1] << "\n";
    std::cout << "  Config Type: " << (int)meta_data.config_type << "\n";
}

bool verifyBootloaderVersion(const uint8_t bootloader_version[2]) {
    if (bootloader_version[0] != BOOTLOADER_VERSION_MAJOR ||
        bootloader_version[1] != BOOTLOADER_VERSION_MINOR) {
        std::cout << "Error: Bootloader version mismatch. Expected "
                  << BOOTLOADER_VERSION_MAJOR << "."
                  << BOOTLOADER_VERSION_MINOR
                  << ", got "
                  << (int)bootloader_version[0] << "."
                  << (int)bootloader_version[1] << "\n";
        return false;
    }

    std::cout << "Bootloader version verified: "
              << BOOTLOADER_VERSION_MAJOR << "."
              << BOOTLOADER_VERSION_MINOR << "\n";
    return true;
}

bool requestMetaData(bluelink::ComponentId destination,
                     bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
    const bluelink::J1939CanIdStruct tx_can_id(
        bluelink::ComponentId::COMPONENT_ID_HLC,
        destination,
        bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY
    );

    const bluelink::J1939CanIdStruct rx_can_id(
        destination,
        bluelink::ComponentId::COMPONENT_ID_HLC,
        bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY
    );

    bluelink::TelemetryPayload::ControllerMetaData request{};
    const int tries = 10;
    bool is_success = false;

    for (int i = 0; i < tries && !is_success; i++) {
        sendCanMessage(
            CONVERT_CAN_ID_TO_UINT32(tx_can_id),
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request)
        );
        if (receiveCanMessage(CONVERT_CAN_ID_TO_UINT32(rx_can_id),
                              reinterpret_cast<uint8_t*>(&meta_data),
                              sizeof(meta_data))) {
            is_success = meta_data.component_id > 0;
        }
        delay(150);
    }

    return is_success;
}

bool requestControllerMetaData(bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
    if (requestMetaData(g_flashed_controller, meta_data)) {
        printMetaData(meta_data);
        return true;
    }

    std::cout << "Failed to receive controller meta data\n";
    return false;
}

bool verifyBootloaderVersionOnDevice(
    const bluelink::TelemetryPayload::ControllerMetaData* cached_meta) {
    if (cached_meta != nullptr && cached_meta->component_id > 0) {
        return verifyBootloaderVersion(cached_meta->bootloader_version);
    }

    bluelink::TelemetryPayload::ControllerMetaData meta_data{};
    if (requestMetaData(bluelink::ComponentId::COMPONENT_ID_BOOTLOADER, meta_data)) {
        printMetaData(meta_data);
        return verifyBootloaderVersion(meta_data.bootloader_version);
    }

    if (requestMetaData(g_flashed_controller, meta_data)) {
        printMetaData(meta_data);
        return verifyBootloaderVersion(meta_data.bootloader_version);
    }

    std::cout << "Failed to read bootloader version from device\n";
    return false;
}

std::string getConfigFilePath(uint8_t config_type) {
    auto it = CONFIG_TYPE_TO_BIN_MAP.find(config_type);
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
    double percent;
    uint32_t data = 0;

    std::cout << "Start Flashing to address 0x" << std::hex << address << std::dec << "\n";
    while (file.read(reinterpret_cast<char*>(&data), sizeof(data))) {
        percent = ((double)(address - g_start_address) / g_file_size) * 100;
        printProgressBar(percent);
        if (!writeFlashData(address, data)) {
            std::cout << "\nFailed to write address " << std::hex << address << "\n";
            return -1;
        }
        address += sizeof(data);
    }

    int remaining_bytes = g_file_size % 4;
    if (remaining_bytes > 0) {
        data = 0;
        file.clear();
        file.seekg(-remaining_bytes, std::ios::end);
        file.read(reinterpret_cast<char*>(&data), remaining_bytes);

        percent = ((double)(address - g_start_address) / g_file_size) * 100;
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

int main(int argc, char* argv[]) {
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printUsage();
        return 0;
    }

    if (argc > 1) {
        g_can_interface = argv[1];
    } else {
        g_can_interface = DEFAULT_CAN_INTERFACE;
    }

    if (argc > 2) {
        std::string controller_type = argv[2];
        auto it = COMPONENT_ID_MAP.find(controller_type);
        if (it != COMPONENT_ID_MAP.end()) {
            g_flashed_controller = it->second;
            std::cout << "Using controller type: " << controller_type << std::endl;
        } else {
            std::cout << "Unknown controller type: " << controller_type << std::endl;
            std::cout << "Using default controller type: gpc" << std::endl;
        }
    }

    if (argc > 3) {
        std::string flash_target = argv[3];
        auto it = FLASH_TARGET_MAP.find(flash_target);
        if (it != FLASH_TARGET_MAP.end()) {
            g_flash_target = it->second;

            if (g_flash_target == FlashTarget::APPLICATION) {
                g_start_address = START_ADDRESS_APPLICATION;
            } else {
                g_start_address = START_ADDRESS_CONFIG;
            }

            std::cout << "Using flash target: " << flash_target
                      << " (Address: 0x" << std::hex << g_start_address << std::dec << ")" << std::endl;
        } else {
            std::cout << "Unknown flash target: " << flash_target << std::endl;
            std::cout << "Using default flash target: app" << std::endl;
        }
    }

    std::string file_name;
    if (argc > 4 && std::string(argv[4]) != "") {
        file_name = argv[4];
    } else if (g_flash_target != FlashTarget::CUSTOMIZED_CONFIG) {
        file_name = DEFAULT_BIN;
    }

    if (!setupCanSocket(g_can_interface)) {
        std::cout << "Can't open CAN interface, good bye\n";
        return -1;
    }

    canFilterByDestConfig(g_can_socket);

    bluelink::TelemetryPayload::ControllerMetaData controller_meta{};
    if (!requestControllerMetaData(controller_meta)) {
        resetCanInterface(g_can_interface);
        requestControllerMetaData(controller_meta);
    }
    const uint8_t config_type = controller_meta.config_type;

    if (g_flash_target == FlashTarget::CUSTOMIZED_CONFIG) {
        if (config_type == 0) {
            std::cout << "Failed to get config type from controller\n";
            close(g_can_socket);
            return -4;
        }

        file_name = getConfigFilePath(config_type);
        if (file_name.empty()) {
            std::cout << "No matching config bin file for config type " << (int)config_type << "\n";
            close(g_can_socket);
            return -5;
        }

        std::cout << "Selected config file: " << file_name << " for config type " << (int)config_type << "\n";
    }

    if (!isFileValid(file_name)) {
        std::cout << "Invalid bin file!\n";
        close(g_can_socket);
        return -3;
    }

    if (!verifyBootloaderVersionOnDevice(&controller_meta)) {
        std::cout << "Bootloader version verification failed\n";
        close(g_can_socket);
        return -6;
    }

    std::cout << "Send start programming request and wait for ACK\n";

    sendStartProgramming();
    delay(500);

    if (!isInProgrammingMode()) {
        std::cout << "Didn't get ACK from controller, trying again\n";
        sendStartProgramming();
        delay(1000);
        if (!isInProgrammingMode()) {
            std::cout << "Didn't get ACK from controller, run sudo bash can_setup.sh and try again\n";
            close(g_can_socket);
            return -2;
        }
    }

    delay(100);
    sendReadyFlashing();
    delay(800);

    if (flashProgram(file_name)) {
        std::cout << "Can't flash bin file! run sudo bash can_setup.sh and try again\n";
        close(g_can_socket);
        return -3;
    }

    close(g_can_socket);
    std::cout << "Good bye!\n";
    return 0;
}
