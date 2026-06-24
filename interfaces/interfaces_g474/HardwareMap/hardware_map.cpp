#include "hardware_map.hpp"

extern CommUartHandle huart2;
extern CommSpiHandle hspi1;
extern CommI2cHandle hi2c1;

namespace HardwareMap {

CommUartHandle& uart_main = huart2;
CommSpiHandle& spi_main = hspi1;
CommI2cHandle& i2c_main = hi2c1;

}  // namespace HardwareMap
