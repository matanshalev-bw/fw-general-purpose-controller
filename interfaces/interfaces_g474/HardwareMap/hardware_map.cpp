#include "hardware_map.hpp"

#ifdef HAL_SPI_MODULE_ENABLED
extern CommSpiHandle hspi1;
#endif
#ifdef HAL_UART_MODULE_ENABLED
extern CommUartHandle huart2;
#endif
#ifdef HAL_I2C_MODULE_ENABLED
extern CommI2cHandle hi2c1;
#endif
#ifdef HAL_TIM_MODULE_ENABLED
extern CommTimHandle htim2;
#endif

namespace HardwareMap {

#ifdef HAL_SPI_MODULE_ENABLED
CommSpiHandle& spi_main = hspi1;
#endif
#ifdef HAL_UART_MODULE_ENABLED
CommUartHandle& uart_main = huart2;
#endif
#ifdef HAL_I2C_MODULE_ENABLED
CommI2cHandle& i2c_main = hi2c1;
#endif
#ifdef HAL_TIM_MODULE_ENABLED
CommTimHandle& pwm_main = htim2;
#endif

}  // namespace HardwareMap
