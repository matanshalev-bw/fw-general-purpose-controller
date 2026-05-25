#ifndef GPC_BLUELINK_USB_CALLBACKS_HPP_
#define GPC_BLUELINK_USB_CALLBACKS_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "bluelink_serializer.hpp"
#include "comm_interrupts_handler.hpp"

class GpcBluelinkUsbCallbacks : public CommInterruptsHandler, public BluelinkCallbacks {
 public:
  static constexpr size_t BUFFER_SIZE = sizeof(bluelink::Serializer::PacketTypes);

  GpcBluelinkUsbCallbacks(std::shared_ptr<CommInterface> comm, HandlePayloadParsinMethodFunctionType parse_payload);

  size_t write(const uint8_t* data, const size_t& size) override;
  uint16_t read(uint8_t* data) override;
};

#endif  // GPC_BLUELINK_USB_CALLBACKS_HPP_
