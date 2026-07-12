#ifndef FW_G474_INTERFACES_HARDWARE_MAP_HPP_
#define FW_G474_INTERFACES_HARDWARE_MAP_HPP_

#include "comm_interface.hpp"
#include "gpio_defines.hpp"

namespace HardwareMap {

constexpr uint8_t MICRO_SEQUENCE_UART_INSTANCE = 2;
constexpr uint8_t MICRO_SEQUENCE_SPI_INSTANCE = 2;
constexpr uint8_t MICRO_SEQUENCE_I2C_INSTANCE = 1;

constexpr GpioPortType WD_EN_PORT = GpioPortType::PORT_A;
constexpr GpioPinNumber WD_EN_PIN = GpioPinNumber::PIN_9;
constexpr GpioPortType WD_KA_PORT = GpioPortType::PORT_A;
constexpr GpioPinNumber WD_KA_PIN = GpioPinNumber::PIN_10;

#ifdef HAL_UART_MODULE_ENABLED
extern CommUartHandle& uart_main;
#endif
extern CommSpiHandle& spi_main;
#ifdef HAL_I2C_MODULE_ENABLED
extern CommI2cHandle& i2c_main;
#endif

}  // namespace HardwareMap

#endif  // FW_G474_INTERFACES_HARDWARE_MAP_HPP_
