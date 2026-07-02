#ifndef FW_G474_INTERFACES_HARDWARE_MAP_HPP_
#define FW_G474_INTERFACES_HARDWARE_MAP_HPP_

#include "comm_interface.hpp"

namespace HardwareMap {

constexpr uint8_t MICRO_SEQUENCE_UART_INSTANCE = 2;
constexpr uint8_t MICRO_SEQUENCE_SPI_INSTANCE = 2;
constexpr uint8_t MICRO_SEQUENCE_I2C_INSTANCE = 1;

#ifdef HAL_UART_MODULE_ENABLED
extern CommUartHandle& uart_main;
#endif
extern CommSpiHandle& spi_main;
#ifdef HAL_I2C_MODULE_ENABLED
extern CommI2cHandle& i2c_main;
#endif

}  // namespace HardwareMap

#endif  // FW_G474_INTERFACES_HARDWARE_MAP_HPP_
