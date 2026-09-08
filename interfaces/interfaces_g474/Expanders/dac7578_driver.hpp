#ifndef FW_G474_EXPANDERS_DAC7578_DRIVER_HPP_
#define FW_G474_EXPANDERS_DAC7578_DRIVER_HPP_

#include <cstdint>

#include "comm_interface.hpp"
#include "expander_interface.hpp"
#include "hardware_map.hpp"
#include "interface_status.hpp"

// DAC7578 12-bit 8-channel I2C DAC.
class Dac7578Driver : public DacExpanderInterface {
 public:
  InterfaceStatus init() override;
  InterfaceStatus writeChannel(uint8_t channel, uint16_t value12) override;

 private:
  static constexpr uint8_t CMD_WRITE_UPDATE = 0x30;
};

#endif  // FW_G474_EXPANDERS_DAC7578_DRIVER_HPP_
