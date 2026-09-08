#ifndef FW_G474_EXPANDERS_ADS7953_DRIVER_HPP_
#define FW_G474_EXPANDERS_ADS7953_DRIVER_HPP_

#include <cstdint>

#include "comm_interface.hpp"
#include "expander_interface.hpp"
#include "gpio_interface.hpp"
#include "hardware_map.hpp"
#include "interface_status.hpp"

// ADS7953 12-bit 16-channel SPI ADC (manual mode with 2-frame pipeline).
class Ads7953Driver : public AdcExpanderInterface {
 public:
  InterfaceStatus init() override;
  InterfaceStatus readChannel(uint8_t channel, uint16_t& raw12) override;
  uint16_t rawToMillivolts(uint16_t raw12) override;

 private:
  static constexpr uint16_t MANUAL_MODE = 0x1000U;
  static constexpr uint16_t PROG_DI06_00 = 0x0800U;

  void spiCs(bool active_low_asserted);
  InterfaceStatus transferFrame(uint16_t tx_word, uint16_t& rx_word);
  static uint16_t buildManualCommand(uint8_t channel);
};

#endif  // FW_G474_EXPANDERS_ADS7953_DRIVER_HPP_
