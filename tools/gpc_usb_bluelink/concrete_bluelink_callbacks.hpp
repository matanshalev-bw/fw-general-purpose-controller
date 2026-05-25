#ifndef GPC_USB_BLUELINK_CONCRETE_BLUELINK_CALLBACKS_HPP_
#define GPC_USB_BLUELINK_CONCRETE_BLUELINK_CALLBACKS_HPP_

#include "bluelink_callbacks.hpp"
#include "serial.h"

constexpr size_t kCommBufferSize = 255;

class ConcreteBluelinkCallbacks : public BluelinkCallbacks {
 public:
  ConcreteBluelinkCallbacks(serial::Serial* serial, HandlePayloadParsinMethodFunctionType parse_payload);
  ~ConcreteBluelinkCallbacks() override = default;

  size_t write(const uint8_t* data, const size_t& size) override;
  uint16_t read(uint8_t* data) override;

 private:
  serial::Serial* serial_;
};

#endif  // GPC_USB_BLUELINK_CONCRETE_BLUELINK_CALLBACKS_HPP_
