#include "concrete_bluelink_callbacks.hpp"

ConcreteBluelinkCallbacks::ConcreteBluelinkCallbacks(serial::Serial* serial,
                                                     HandlePayloadParsinMethodFunctionType parse_payload,
                                                     bool open_serial)
    : BluelinkCallbacks(parse_payload), serial_(serial) {
  if (open_serial) {
    serial_->open();
  }
}

size_t ConcreteBluelinkCallbacks::write(const uint8_t* data, const size_t& size) {
  serial_->write(data, size);
  return size;
}

uint16_t ConcreteBluelinkCallbacks::read(uint8_t* data) {
  uint16_t i = 0;
  for (; serial_->available() > 0 && i < kCommBufferSize; ++i) {
    serial_->read(&data[i], 1);
  }
  return i;
}
