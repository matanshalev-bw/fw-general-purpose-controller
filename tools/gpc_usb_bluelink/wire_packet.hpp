#ifndef GPC_USB_BLUELINK_WIRE_PACKET_HPP_
#define GPC_USB_BLUELINK_WIRE_PACKET_HPP_

#include <cstddef>
#include <cstdint>

#include "bluelink_serializer.hpp"

namespace gpc_usb_bluelink {

constexpr size_t kMaxWirePayloadBytes = 240;

struct SendOptions {
  uint8_t source_id = bluelink::HLC_ADDRESS;
  uint8_t destination_id = 0x11;  // COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER
  bluelink::PayloadTypeIds payload_type = bluelink::PayloadTypeIds::UNKNOWN;
  bluelink::QosTypes qos = bluelink::QosTypes::NONE;
  uint32_t packet_sequence_id = 0;
};

// Builds a full BlueLink wire frame (prefix + header + payload + suffix). Returns total byte count or 0 on error.
size_t buildWirePacket(uint8_t* buffer, size_t buffer_capacity, const SendOptions& options, const uint8_t* payload,
                       size_t payload_size);

}  // namespace gpc_usb_bluelink

#endif  // GPC_USB_BLUELINK_WIRE_PACKET_HPP_
