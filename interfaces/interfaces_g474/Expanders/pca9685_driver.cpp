#include "pca9685_driver.hpp"

InterfaceStatus Pca9685Driver::writeReg(uint8_t reg, uint8_t value) {
#ifdef HAL_I2C_MODULE_ENABLED
  CommI2c i2c(&HardwareMap::i2c_main, 100U);
  i2c.setDeviceAddr(static_cast<uint16_t>(HardwareMap::PCA9685_I2C_ADDR << 1));
  const uint8_t buf[2] = {reg, value};
  return i2c.write(buf, 2);
#else
  (void)reg;
  (void)value;
  return InterfaceStatus::INTERFACE_ERROR;
#endif
}

InterfaceStatus Pca9685Driver::writeRegs(uint8_t start_reg, const uint8_t* data, uint8_t length) {
#ifdef HAL_I2C_MODULE_ENABLED
  if (data == nullptr || length == 0 || length > 4) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  uint8_t buf[5] = {start_reg};
  for (uint8_t i = 0; i < length; ++i) {
    buf[1U + i] = data[i];
  }
  CommI2c i2c(&HardwareMap::i2c_main, 100U);
  i2c.setDeviceAddr(static_cast<uint16_t>(HardwareMap::PCA9685_I2C_ADDR << 1));
  return i2c.write(buf, static_cast<uint16_t>(1U + length));
#else
  (void)start_reg;
  (void)data;
  (void)length;
  return InterfaceStatus::INTERFACE_ERROR;
#endif
}

InterfaceStatus Pca9685Driver::setFrequency(uint32_t frequency_hz) {
  if (frequency_hz < 24U) {
    frequency_hz = 24U;
  }
  if (frequency_hz > 1526U) {
    frequency_hz = 1526U;
  }

  uint32_t prescale = (OSC_HZ + (2048U * frequency_hz)) / (4096U * frequency_hz);
  if (prescale < 3U) {
    prescale = 3U;
  }
  if (prescale > 255U) {
    prescale = 255U;
  }

  if (writeReg(REG_MODE1, static_cast<uint8_t>(MODE1_SLEEP | MODE1_AI)) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (writeReg(REG_PRESCALE, static_cast<uint8_t>(prescale)) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (writeReg(REG_MODE1, MODE1_AI) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  HAL_Delay(1);
  return writeReg(REG_MODE1, static_cast<uint8_t>(MODE1_RESTART | MODE1_AI));
}

InterfaceStatus Pca9685Driver::init() {
  if (writeReg(REG_MODE2, 0x04) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return setFrequency(200U);
}

InterfaceStatus Pca9685Driver::setPwm(uint8_t channel, uint32_t frequency_hz, uint16_t duty_percent) {
  if (channel > 15U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (duty_percent > 100U) {
    duty_percent = 100U;
  }
  if (frequency_hz == 0U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  if (setFrequency(frequency_hz) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  uint16_t on = 0;
  uint16_t off = 0;
  if (duty_percent == 0U) {
    on = 0;
    off = 0x1000U;
  } else if (duty_percent >= 100U) {
    on = 0x1000U;
    off = 0;
  } else {
    off = static_cast<uint16_t>((static_cast<uint32_t>(duty_percent) * 4095U) / 100U);
  }

  const uint8_t data[4] = {static_cast<uint8_t>(on & 0xFFU), static_cast<uint8_t>((on >> 8) & 0xFFU),
                           static_cast<uint8_t>(off & 0xFFU), static_cast<uint8_t>((off >> 8) & 0xFFU)};
  const uint8_t reg = static_cast<uint8_t>(REG_LED0_ON_L + (4U * channel));
  return writeRegs(reg, data, 4);
}
