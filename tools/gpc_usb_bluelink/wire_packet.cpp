#include "wire_packet.hpp"

#include <cstring>

#include "checksum_32.hpp"

namespace gpc_usb_bluelink {

size_t buildWirePacket(uint8_t* buffer, size_t buffer_capacity, const SendOptions& options, const uint8_t* payload,
                       size_t payload_size) {
  if (buffer == nullptr || payload_size > kMaxWirePayloadBytes) {
    return 0;
  }

  const size_t header_payload_len = sizeof(bluelink::Header) + payload_size;
  const size_t total_size = sizeof(bluelink::Prefix) + header_payload_len + sizeof(bluelink::Suffix);
  if (total_size > buffer_capacity) {
    return 0;
  }

  bluelink::Prefix prefix{};
  memcpy(prefix.sign, bluelink::PACKET_SIGN, sizeof(bluelink::PACKET_SIGN));
  prefix.length = static_cast<uint8_t>(header_payload_len);

  bluelink::Header header{};
  header.source_id = options.source_id;
  header.destination_id = options.destination_id;
  header.packet_sequence_id = options.packet_sequence_id;
  header.qos_type = options.qos;
  header.payload_type = options.payload_type;

  bluelink::Suffix suffix{};
  suffix.checksum = Checksum32::CalculateChecksum(reinterpret_cast<const uint8_t*>(&header), header_payload_len);

  size_t offset = 0;
  memcpy(buffer + offset, &prefix, sizeof(prefix));
  offset += sizeof(prefix);
  memcpy(buffer + offset, &header, sizeof(header));
  offset += sizeof(header);
  if (payload_size > 0 && payload != nullptr) {
    memcpy(buffer + offset, payload, payload_size);
    offset += payload_size;
  }
  memcpy(buffer + offset, &suffix, sizeof(suffix));
  offset += sizeof(suffix);

  return offset;
}

}  // namespace gpc_usb_bluelink
