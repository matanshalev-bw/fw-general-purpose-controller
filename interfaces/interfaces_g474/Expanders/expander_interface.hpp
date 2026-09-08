#ifndef FW_G474_EXPANDERS_EXPANDER_INTERFACE_HPP_
#define FW_G474_EXPANDERS_EXPANDER_INTERFACE_HPP_

#include <cstdint>

#include "interface_status.hpp"

// Abstract expanders — application code depends only on these types.
class GpioExpanderInterface {
 public:
  virtual ~GpioExpanderInterface() = default;
  virtual InterfaceStatus init() = 0;
  virtual InterfaceStatus digitalWrite(uint8_t port /*1=A,2=B*/, uint8_t pin /*0-7*/, bool value) = 0;
  virtual InterfaceStatus digitalRead(uint8_t port, uint8_t pin, bool& value) = 0;
};

class AdcExpanderInterface {
 public:
  virtual ~AdcExpanderInterface() = default;
  virtual InterfaceStatus init() = 0;
  virtual InterfaceStatus readChannel(uint8_t channel, uint16_t& raw12) = 0;
  virtual uint16_t rawToMillivolts(uint16_t raw12) = 0;
};

class DacExpanderInterface {
 public:
  virtual ~DacExpanderInterface() = default;
  virtual InterfaceStatus init() = 0;
  virtual InterfaceStatus writeChannel(uint8_t channel, uint16_t value12) = 0;
};

class PwmExpanderInterface {
 public:
  virtual ~PwmExpanderInterface() = default;
  virtual InterfaceStatus init() = 0;
  virtual InterfaceStatus setPwm(uint8_t channel, uint32_t frequency_hz, uint16_t duty_percent) = 0;
};

// Board-facing accessors; chip selection stays inside the interface layer.
class ExpanderInterface {
 public:
  enum class GpioKind : uint8_t { I2C = 0, SPI = 1 };

  static InterfaceStatus initAll();
  static GpioExpanderInterface& gpio(GpioKind kind);
  static AdcExpanderInterface& adc();
  static DacExpanderInterface& dac();
  static PwmExpanderInterface& pwm();
};

#endif  // FW_G474_EXPANDERS_EXPANDER_INTERFACE_HPP_
