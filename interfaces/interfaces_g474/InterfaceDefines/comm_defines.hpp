#ifndef COMM_DEFINES_HPP_
#define COMM_DEFINES_HPP_

#include "stm32g4xx_hal.h"
#include "usbd_def.h"

using CommCanHandle = FDCAN_HandleTypeDef;
using CommCanTxHeader = FDCAN_TxHeaderTypeDef;
using CommCanRxHeader = FDCAN_RxHeaderTypeDef;
using CommUsbHandle = USBD_HandleTypeDef;

#ifdef HAL_UART_MODULE_ENABLED
using CommUartHandle = UART_HandleTypeDef;
#endif

#ifdef HAL_I2C_MODULE_ENABLED
using CommI2cHandle = I2C_HandleTypeDef;
#endif

#ifdef HAL_SPI_MODULE_ENABLED
using CommSpiHandle = SPI_HandleTypeDef;
#endif

#ifdef HAL_DMA_MODULE_ENABLED
using CommDmaHandle = DMA_HandleTypeDef;
#endif

#ifdef HAL_ADC_MODULE_ENABLED
using CommAdcHandle = ADC_HandleTypeDef;
#endif

#ifdef HAL_DAC_MODULE_ENABLED
using CommDacHandle = DAC_HandleTypeDef;
#endif

#ifdef HAL_TIM_MODULE_ENABLED
using CommTimHandle = TIM_HandleTypeDef;
#endif

#endif  // COMM_DEFINES_HPP_
