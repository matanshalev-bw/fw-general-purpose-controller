#ifndef FW_G474_EXPANDERS_MCP23X17_DRIVER_HPP_
#define FW_G474_EXPANDERS_MCP23X17_DRIVER_HPP_

#include <cstdint>

#include "comm_interface.hpp"
#include "expander_interface.hpp"
#include "gpio_interface.hpp"
#include "hardware_map.hpp"
#include "interface_status.hpp"

// MCP23017 (I2C) / MCP23S17 (SPI) 16-bit GPIO expander, BANK=0 register map.
class Mcp23x17Driver : public GpioExpanderInterface {
 public:
  enum class Bus : uint8_t { I2C = 0, SPI = 1 };

  explicit Mcp23x17Driver(Bus bus);

  InterfaceStatus init() override;
  InterfaceStatus digitalWrite(uint8_t port, uint8_t pin, bool value) override;
  InterfaceStatus digitalRead(uint8_t port, uint8_t pin, bool& value) override;

 private:
  static constexpr uint8_t REG_IODIRA = 0x00;
  static constexpr uint8_t REG_IODIRB = 0x01;
  static constexpr uint8_t REG_IOCON = 0x0A;
  static constexpr uint8_t REG_GPIOA = 0x12;
  static constexpr uint8_t REG_GPIOB = 0x13;
  static constexpr uint8_t REG_OLATA = 0x14;
  static constexpr uint8_t REG_OLATB = 0x15;

  static constexpr uint8_t SPI_WRITE_OPCODE = 0x40;
  static constexpr uint8_t SPI_READ_OPCODE = 0x41;

  Bus bus_;

  static uint8_t iodirReg(uint8_t port);
  static uint8_t gpioReg(uint8_t port);
  static uint8_t olatReg(uint8_t port);
  static bool validPortPin(uint8_t port, uint8_t pin);

  InterfaceStatus writeReg(uint8_t reg, uint8_t value);
  InterfaceStatus readReg(uint8_t reg, uint8_t& value);
  InterfaceStatus updateBit(uint8_t reg, uint8_t bit, bool set);
  InterfaceStatus spiTransfer(const uint8_t* tx, uint8_t* rx, uint16_t len);
  void spiCs(bool active_low_asserted);
};

#endif  // FW_G474_EXPANDERS_MCP23X17_DRIVER_HPP_
