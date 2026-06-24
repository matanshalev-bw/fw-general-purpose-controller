#ifndef FW_G474_INTERFACES_HARDWARE_MAP_HPP_
#define FW_G474_INTERFACES_HARDWARE_MAP_HPP_

#include "comm_interface.hpp"

namespace HardwareMap {

constexpr uint8_t MICRO_SEQUENCE_UART_INSTANCE = 2;
constexpr uint8_t MICRO_SEQUENCE_SPI_INSTANCE = 2;
constexpr uint8_t MICRO_SEQUENCE_I2C_INSTANCE = 1;

extern CommUartHandle& uart_main;
extern CommSpiHandle& spi_main;
extern CommI2cHandle& i2c_main;

}  // namespace HardwareMap

#endif  // FW_G474_INTERFACES_HARDWARE_MAP_HPP_
