#include "concrete_bluelink_callbacks.hpp"

ConcreteBluelinkCallbacks::ConcreteBluelinkCallbacks(serial::Serial* serial,
                                                     HandlePayloadParsinMethodFunctionType parse_payload,
                                                     bool open_on_construct)
    : BluelinkCallbacks(parse_payload), serial_(serial) {
  if (open_on_construct) {
    serial_->open();
  }
}

size_t ConcreteBluelinkCallbacks::write(const uint8_t* data, const size_t& size) {
  if (serial_ == nullptr) {
    return 0;
  }

  try {
    if (!serial_->isOpen()) {
      return 0;
    }
    serial_->write(data, size);
    return size;
  } catch (const std::exception&) {
    return 0;
  }
}

uint16_t ConcreteBluelinkCallbacks::read(uint8_t* data) {
  if (serial_ == nullptr) {
    return 0;
  }

  try {
    if (!serial_->isOpen()) {
      return 0;
    }

    uint16_t i = 0;
    for (; serial_->available() > 0 && i < kCommBufferSize; ++i) {
      serial_->read(&data[i], 1);
    }
    return i;
  } catch (const std::exception&) {
    return 0;
  }
}
