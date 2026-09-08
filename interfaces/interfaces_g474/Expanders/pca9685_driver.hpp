#ifndef FW_G474_EXPANDERS_PCA9685_DRIVER_HPP_
#define FW_G474_EXPANDERS_PCA9685_DRIVER_HPP_

#include <cstdint>

#include "comm_interface.hpp"
#include "expander_interface.hpp"
#include "hardware_map.hpp"
#include "interface_status.hpp"
#include "stm32g4xx_hal.h"

// PCA9685 16-channel 12-bit PWM I2C controller.
class Pca9685Driver : public PwmExpanderInterface {
 public:
  InterfaceStatus init() override;
  InterfaceStatus setPwm(uint8_t channel, uint32_t frequency_hz, uint16_t duty_percent) override;

 private:
  static constexpr uint8_t REG_MODE1 = 0x00;
  static constexpr uint8_t REG_MODE2 = 0x01;
  static constexpr uint8_t REG_LED0_ON_L = 0x06;
  static constexpr uint8_t REG_PRESCALE = 0xFE;

  static constexpr uint8_t MODE1_SLEEP = 0x10;
  static constexpr uint8_t MODE1_AI = 0x20;
  static constexpr uint8_t MODE1_RESTART = 0x80;
  static constexpr uint32_t OSC_HZ = 25000000U;

  InterfaceStatus writeReg(uint8_t reg, uint8_t value);
  InterfaceStatus writeRegs(uint8_t start_reg, const uint8_t* data, uint8_t length);
  InterfaceStatus setFrequency(uint32_t frequency_hz);
};

#endif  // FW_G474_EXPANDERS_PCA9685_DRIVER_HPP_
