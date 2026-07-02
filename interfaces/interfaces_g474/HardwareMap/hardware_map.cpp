#include "hardware_map.hpp"

extern CommSpiHandle hspi1;
#ifdef HAL_UART_MODULE_ENABLED
extern CommUartHandle huart2;
#endif
#ifdef HAL_I2C_MODULE_ENABLED
extern CommI2cHandle hi2c1;
#endif

namespace HardwareMap {

CommSpiHandle& spi_main = hspi1;
#ifdef HAL_UART_MODULE_ENABLED
CommUartHandle& uart_main = huart2;
#endif
#ifdef HAL_I2C_MODULE_ENABLED
CommI2cHandle& i2c_main = hi2c1;
#endif

}  // namespace HardwareMap
