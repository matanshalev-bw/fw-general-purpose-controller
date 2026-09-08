#include "ads7953_driver.hpp"

void Ads7953Driver::spiCs(bool active_low_asserted) {
  const GpioPin cs =
      GpioInterface::createDigitalGpio(HardwareMap::ADS7953_CS_PORT, HardwareMap::ADS7953_CS_PIN);
  GpioInterface::digitalWrite(cs, active_low_asserted ? GpioPinState::PIN_RESET : GpioPinState::PIN_SET);
}

uint16_t Ads7953Driver::buildManualCommand(uint8_t channel) {
  return static_cast<uint16_t>(MANUAL_MODE | PROG_DI06_00 | ((static_cast<uint16_t>(channel) & 0x0FU) << 7));
}

InterfaceStatus Ads7953Driver::transferFrame(uint16_t tx_word, uint16_t& rx_word) {
#ifdef HAL_SPI_MODULE_ENABLED
  const uint8_t tx[2] = {static_cast<uint8_t>((tx_word >> 8) & 0xFFU), static_cast<uint8_t>(tx_word & 0xFFU)};
  uint8_t rx[2] = {};
  CommSpi spi(&HardwareMap::spi_main, 100U);
  spiCs(true);
  const InterfaceStatus status = spi.transmitReceive(tx, rx, 2);
  spiCs(false);
  if (status != InterfaceStatus::INTERFACE_OK) {
    return status;
  }
  rx_word = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8) | rx[1]);
  return InterfaceStatus::INTERFACE_OK;
#else
  (void)tx_word;
  (void)rx_word;
  return InterfaceStatus::INTERFACE_ERROR;
#endif
}

InterfaceStatus Ads7953Driver::init() {
  spiCs(false);
  uint16_t unused = 0;
  if (transferFrame(0x0000U, unused) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return transferFrame(0x0000U, unused);
}

InterfaceStatus Ads7953Driver::readChannel(uint8_t channel, uint16_t& raw12) {
  if (channel > 15U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  const uint16_t cmd = buildManualCommand(channel);
  uint16_t rx = 0;
  if (transferFrame(cmd, rx) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (transferFrame(0x0000U, rx) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  if (transferFrame(0x0000U, rx) != InterfaceStatus::INTERFACE_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  raw12 = static_cast<uint16_t>(rx & 0x0FFFU);
  return InterfaceStatus::INTERFACE_OK;
}

uint16_t Ads7953Driver::rawToMillivolts(uint16_t raw12) {
  return static_cast<uint16_t>((static_cast<uint32_t>(raw12) * HardwareMap::ADS7953_VREF_MV) / 4095U);
}
