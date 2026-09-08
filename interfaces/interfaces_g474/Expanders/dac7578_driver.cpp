#include "dac7578_driver.hpp"

InterfaceStatus Dac7578Driver::init() { return InterfaceStatus::INTERFACE_OK; }

InterfaceStatus Dac7578Driver::writeChannel(uint8_t channel, uint16_t value12) {
#ifdef HAL_I2C_MODULE_ENABLED
  if (channel > 7U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (value12 > 0x0FFFU) {
    value12 = 0x0FFFU;
  }

  const uint8_t cmd = static_cast<uint8_t>(CMD_WRITE_UPDATE | (channel & 0x07U));
  const uint8_t msb = static_cast<uint8_t>((value12 >> 4) & 0xFFU);
  const uint8_t lsb = static_cast<uint8_t>((value12 & 0x0FU) << 4);
  const uint8_t buf[3] = {cmd, msb, lsb};

  CommI2c i2c(&HardwareMap::i2c_main, 100U);
  i2c.setDeviceAddr(static_cast<uint16_t>(HardwareMap::DAC7578_I2C_ADDR << 1));
  return i2c.write(buf, 3);
#else
  (void)channel;
  (void)value12;
  return InterfaceStatus::INTERFACE_ERROR;
#endif
}
