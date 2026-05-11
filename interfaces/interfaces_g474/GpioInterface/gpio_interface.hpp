/*
 * gpio_interface.hpp
 *
 * Created on: Jan 16, 2025
 * Author: ariel
 */

#ifndef INTERFACE_GPIO_INTERFACE_HPP_
#define INTERFACE_GPIO_INTERFACE_HPP_

#include "gpio_defines.hpp"
#include "interface_status.hpp"
#include "stm32g4xx_hal.h"
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32g4xx_ll_adc.h"
#include "../AdcManager/adc_manager.hpp"
#endif

class GpioInterface {
private:
    static GPIO_TypeDef* getGpioPort(GpioPortType port);
#ifdef HAL_ADC_MODULE_ENABLED
    static float adcToVoltage(uint16_t adc_value, float voltage_divider_ratio, ADC_HandleTypeDef* hadc);
#endif

public:
    static GpioPin createDigitalGpio(GpioPortType port, GpioPinNumber pin);
    static GpioPin createDigitalGpio(GPIO_TypeDef* gpio_port, uint16_t hal_pin);
    
    static InterfaceStatus digitalWrite(const GpioPin& gpio_pin, GpioPinState state);
    static InterfaceStatus digitalRead(const GpioPin& gpio_pin, GpioPinState& state);
    static InterfaceStatus digitalToggle(const GpioPin& gpio_pin);
    
#ifdef HAL_ADC_MODULE_ENABLED
    static AnalogGpio createAnalogGpio(uint32_t adc_channel, float voltage_divider_ratio, ADC_HandleTypeDef* hadc);
    static InterfaceStatus analogRead(const AnalogGpio& analog_gpio, float& voltage);
    static InterfaceStatus analogReadDma(AdcInstance adc_instance, uint8_t channel_index, float& voltage);
    static InterfaceStatus calibrateAdc(ADC_HandleTypeDef* hadc);
    static InterfaceStatus initializeAdcDma(ADC_HandleTypeDef* hadc1, ADC_HandleTypeDef* hadc2, ADC_HandleTypeDef* hadc3,
                                          DMA_HandleTypeDef* hdma_adc1, DMA_HandleTypeDef* hdma_adc2, DMA_HandleTypeDef* hdma_adc3);
    static InterfaceStatus startAdcDma();
    static InterfaceStatus stopAdcDma();
    
#endif
    
    static GpioPortType getPortType(GPIO_TypeDef* gpio_port);
};

#endif /* INTERFACE_GPIO_INTERFACE_HPP_ */
