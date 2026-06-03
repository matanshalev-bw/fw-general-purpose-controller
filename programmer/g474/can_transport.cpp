#include "can_transport.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "distributed_can_id.hpp"

namespace {

constexpr int kCanRxTimeoutMs = 800;

void canFilterByDestConfig(int socket_fd) {
  struct can_filter rfilter[1];

  const uint32_t destination_mask = 0xFF << 8;
  const uint32_t destination_value = static_cast<uint32_t>(bluelink::ComponentId::COMPONENT_ID_HLC) << 8;

  rfilter[0].can_id = destination_value | CAN_EFF_FLAG;
  rfilter[0].can_mask = destination_mask | CAN_EFF_FLAG;

  setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
}

uint32_t buildCanId(bluelink::ComponentId source, bluelink::ComponentId destination,
                    bluelink::PayloadTypeIds payload_type) {
  const bluelink::J1939CanIdStruct can_id(source, destination, payload_type);
  return CONVERT_CAN_ID_TO_UINT32(can_id);
}

}  // namespace

CanTransport::CanTransport(std::string can_interface) : can_interface_(std::move(can_interface)) {}

bool CanTransport::init() {
  can_socket_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (can_socket_ < 0) {
    perror("Socket");
    return false;
  }

  struct ifreq ifr;
  std::strcpy(ifr.ifr_name, can_interface_.c_str());
  if (ioctl(can_socket_, SIOCGIFINDEX, &ifr) < 0) {
    perror("ioctl");
    close(can_socket_);
    can_socket_ = -1;
    return false;
  }

  struct sockaddr_can addr;
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;
  if (bind(can_socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("Bind");
    close(can_socket_);
    can_socket_ = -1;
    return false;
  }

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = kCanRxTimeoutMs * 1000;
  if (setsockopt(can_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt timeout");
  }

  canFilterByDestConfig(can_socket_);
  return true;
}

void CanTransport::shutdown() {
  if (can_socket_ >= 0) {
    close(can_socket_);
    can_socket_ = -1;
  }
}

bool CanTransport::reopenAfterReset() {
  shutdown();
  return init();
}

void CanTransport::sendCanMessage(uint32_t can_id, const uint8_t* data, size_t size) {
  struct can_frame frame;

  size_t data_len = size;
  if (data_len > 8) {
    data_len = 8;
  }

  frame.can_id = can_id | CAN_EFF_FLAG;
  frame.can_dlc = data_len;
  std::memcpy(frame.data, data, data_len);

  write(can_socket_, &frame, sizeof(frame));
}

bool CanTransport::receiveCanMessage(uint32_t expected_addr, uint8_t* data, size_t size) {
  struct can_frame frame;
  const int nbytes = read(can_socket_, &frame, sizeof(frame));

  if (nbytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;
    }
    perror("Read from CAN");
    return false;
  }

  if (static_cast<size_t>(nbytes) != sizeof(frame)) {
    std::cerr << "Received incomplete CAN frame\n";
    return false;
  }

  if ((frame.can_id & CAN_EFF_MASK) == (expected_addr & CAN_EFF_MASK)) {
    const size_t copy_size = (frame.can_dlc < size) ? frame.can_dlc : size;
    std::memcpy(data, frame.data, copy_size);
    return true;
  }

  return false;
}

bool CanTransport::requestMetaData(bluelink::ComponentId destination,
                                   bluelink::TelemetryPayload::ControllerMetaData& meta_data) {
  const uint32_t tx_can_id = buildCanId(bluelink::ComponentId::COMPONENT_ID_HLC, destination,
                                        bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY);
  const uint32_t rx_can_id = buildCanId(destination, bluelink::ComponentId::COMPONENT_ID_HLC,
                                        bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY);

  bluelink::TelemetryPayload::ControllerMetaData request{};
  const int tries = 10;

  for (int i = 0; i < tries; ++i) {
    sendCanMessage(tx_can_id, reinterpret_cast<const uint8_t*>(&request), sizeof(request));
    if (receiveCanMessage(rx_can_id, reinterpret_cast<uint8_t*>(&meta_data), sizeof(meta_data))) {
      if (meta_data.component_id > 0) {
        return true;
      }
    }
    usleep(150000);
  }

  return false;
}

bool CanTransport::sendProgrammingCommand(bluelink::ComponentId destination,
                                          const bluelink::CommandsPayload::ProgrammingCommand& cmd) {
  const uint32_t tx_can_id = buildCanId(bluelink::ComponentId::COMPONENT_ID_HLC, destination,
                                        bluelink::PayloadTypeIds::PROGRAMMING_COMMAND);
  sendCanMessage(tx_can_id, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
  return true;
}

bool CanTransport::receiveProgrammingCommand(bluelink::ComponentId expected_source,
                                             bluelink::CommandsPayload::ProgrammingCommand& cmd,
                                             int timeout_ms) {
  const uint32_t rx_can_id = buildCanId(expected_source, bluelink::ComponentId::COMPONENT_ID_HLC,
                                        bluelink::PayloadTypeIds::PROGRAMMING_COMMAND);

  const int step_ms = 10;
  int elapsed = 0;
  while (elapsed < timeout_ms) {
    if (receiveCanMessage(rx_can_id, reinterpret_cast<uint8_t*>(&cmd), sizeof(cmd))) {
      return true;
    }
    usleep(step_ms * 1000);
    elapsed += step_ms;
  }

  return false;
}
