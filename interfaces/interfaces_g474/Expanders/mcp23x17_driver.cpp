#include "mcp23x17_driver.hpp"

Mcp23x17Driver::Mcp23x17Driver(Bus bus) : bus_(bus) {}

uint8_t Mcp23x17Driver::iodirReg(uint8_t port) { return port == 2 ? REG_IODIRB : REG_IODIRA; }
uint8_t Mcp23x17Driver::gpioReg(uint8_t port) { return port == 2 ? REG_GPIOB : REG_GPIOA; }
uint8_t Mcp23x17Driver::olatReg(uint8_t port) { return port == 2 ? REG_OLATB : REG_OLATA; }

bool Mcp23x17Driver::validPortPin(uint8_t port, uint8_t pin) {
  return (port == 1 || port == 2) && pin <= 7;
}

void Mcp23x17Driver::spiCs(bool active_low_asserted) {
  const GpioPin cs =
      GpioInterface::createDigitalGpio(HardwareMap::MCP23S17_CS_PORT, HardwareMap::MCP23S17_CS_PIN);
  GpioInterface::digitalWrite(cs, active_low_asserted ? GpioPinState::PIN_RESET : GpioPinState::PIN_SET);
}

InterfaceStatus Mcp23x17Driver::spiTransfer(const uint8_t* tx, uint8_t* rx, uint16_t len) {
#ifdef HAL_SPI_MODULE_ENABLED
  CommSpi spi(&HardwareMap::spi_main, 100U);
  spiCs(true);
  const InterfaceStatus status = spi.transmitReceive(tx, rx, len);
  spiCs(false);
  return status;
#else
  (void)tx;
  (void)rx;
  (void)len;
  return InterfaceStatus::INTERFACE_ERROR;
#endif
}

InterfaceStatus Mcp23x17Driver::writeReg(uint8_t reg, uint8_t value) {
  if (bus_ == Bus::I2C) {
#ifdef HAL_I2C_MODULE_ENABLED
    CommI2c i2c(&HardwareMap::i2c_main, 100U);
    i2c.setDeviceAddr(static_cast<uint16_t>(HardwareMap::MCP23017_I2C_ADDR << 1));
    const uint8_t buf[2] = {reg, value};
    return i2c.write(buf, 2);
#else
    (void)reg;
    (void)value;
    return InterfaceStatus::INTERFACE_ERROR;
#endif
  }

  const uint8_t opcode =
      static_cast<uint8_t>(SPI_WRITE_OPCODE | ((HardwareMap::MCP23S17_SPI_ADDR & 0x07U) << 1));
  const uint8_t tx[3] = {opcode, reg, value};
  uint8_t rx[3] = {};
  return spiTransfer(tx, rx, 3);
}

InterfaceStatus Mcp23x17Driver::readReg(uint8_t reg, uint8_t& value) {
  if (bus_ == Bus::I2C) {
#ifdef HAL_I2C_MODULE_ENABLED
    CommI2c i2c(&HardwareMap::i2c_main, 100U);
    i2c.setDeviceAddr(static_cast<uint16_t>(HardwareMap::MCP23017_I2C_ADDR << 1));
    if (i2c.write(&reg, 1) != InterfaceStatus::INTERFACE_OK) {
      return InterfaceStatus::INTERFACE_ERROR;
    }
    return i2c.read(&value, 1);
#else
    (void)reg;
    (void)value;
    return InterfaceStatus::INTERFACE_ERROR;
#endif
  }

  const uint8_t opcode =
      static_cast<uint8_t>(SPI_READ_OPCODE | ((HardwareMap::MCP23S17_SPI_ADDR & 0x07U) << 1));
  const uint8_t tx[3] = {opcode, reg, 0x00};
  uint8_t rx[3] = {};
  if (spiTransfer(tx, rx, 3) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  value = rx[2];
  return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus Mcp23x17Driver::updateBit(uint8_t reg, uint8_t bit, bool set) {
  uint8_t current = 0;
  if (readReg(reg, current) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (set) {
    current = static_cast<uint8_t>(current | (1U << bit));
  } else {
    current = static_cast<uint8_t>(current & static_cast<uint8_t>(~(1U << bit)));
  }
  return writeReg(reg, current);
}

InterfaceStatus Mcp23x17Driver::init() {
  const uint8_t iocon = (bus_ == Bus::SPI) ? 0x08U : 0x00U;
  if (writeReg(REG_IOCON, iocon) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (writeReg(REG_IODIRA, 0xFF) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return writeReg(REG_IODIRB, 0xFF);
}

InterfaceStatus Mcp23x17Driver::digitalWrite(uint8_t port, uint8_t pin, bool value) {
  if (!validPortPin(port, pin)) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (updateBit(iodirReg(port), pin, false) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return updateBit(olatReg(port), pin, value);
}

InterfaceStatus Mcp23x17Driver::digitalRead(uint8_t port, uint8_t pin, bool& value) {
  if (!validPortPin(port, pin)) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (updateBit(iodirReg(port), pin, true) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  uint8_t gpio = 0;
  if (readReg(gpioReg(port), gpio) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  value = ((gpio >> pin) & 0x01U) != 0;
  return InterfaceStatus::INTERFACE_OK;
}
