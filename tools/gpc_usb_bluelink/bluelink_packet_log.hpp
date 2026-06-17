#ifndef GPC_USB_BLUELINK_BLUELINK_PACKET_LOG_HPP_
#define GPC_USB_BLUELINK_BLUELINK_PACKET_LOG_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "PayloadTypes.hpp"

class BluelinkPacketLog {
 public:
  void feed(const uint8_t* data, size_t len);

 private:
  std::vector<uint8_t> buffer_;

  void tryParsePackets();
  void emitPacket(const uint8_t* packet, size_t total_len);
};

std::string payloadTypeName(bluelink::PayloadTypeIds type);

#endif  // GPC_USB_BLUELINK_BLUELINK_PACKET_LOG_HPP_
