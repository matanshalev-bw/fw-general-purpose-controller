/*
 * gpio_defines.hpp
 *
 * Created on: Jan 16, 2025
 * Author: ariel
 */

#ifndef INTERFACE_GPIO_DEFINES_HPP_
#define INTERFACE_GPIO_DEFINES_HPP_

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"

enum class GpioPortType : uint8_t {
    UNKNOWN_PORT = 0,
    PORT_A = 1,
    PORT_B = 2,
    PORT_C = 3,
    PORT_D = 4,
    PORT_E = 5,
    PORT_F = 6
};

enum class GpioPinState : uint8_t {
    PIN_RESET = 0,
    PIN_SET = 1
};

enum class GpioPinNumber : uint16_t {
    PIN_0 = GPIO_PIN_0,
    PIN_1 = GPIO_PIN_1,
    PIN_2 = GPIO_PIN_2,
    PIN_3 = GPIO_PIN_3,
    PIN_4 = GPIO_PIN_4,
    PIN_5 = GPIO_PIN_5,
    PIN_6 = GPIO_PIN_6,
    PIN_7 = GPIO_PIN_7,
    PIN_8 = GPIO_PIN_8,
    PIN_9 = GPIO_PIN_9,
    PIN_10 = GPIO_PIN_10,
    PIN_11 = GPIO_PIN_11,
    PIN_12 = GPIO_PIN_12,
    PIN_13 = GPIO_PIN_13,
    PIN_14 = GPIO_PIN_14,
    PIN_15 = GPIO_PIN_15
};

struct GpioPin {
    GpioPortType port;
    uint16_t pin;
    
    GpioPin() : port(GpioPortType::UNKNOWN_PORT), pin(0) {}
    GpioPin(GpioPortType p, uint16_t pin_num) : port(p), pin(pin_num) {}
    
    bool isValid() const { return port != GpioPortType::UNKNOWN_PORT; }
};

#ifdef HAL_ADC_MODULE_ENABLED
struct AnalogGpio {
    uint32_t channel;
    float voltage_divider_ratio;
    ADC_HandleTypeDef* adc_handler;
    
    AnalogGpio() : channel(0), voltage_divider_ratio(1.0f), adc_handler(nullptr) {}
    AnalogGpio(uint32_t adc_channel, float divider_ratio, ADC_HandleTypeDef* hadc) 
        : channel(adc_channel), voltage_divider_ratio(divider_ratio), adc_handler(hadc) {}
};
#endif

constexpr GpioPinNumber toGpioPinNumber(uint16_t hal_pin) {
    switch (hal_pin) {
        case GPIO_PIN_0: return GpioPinNumber::PIN_0;
        case GPIO_PIN_1: return GpioPinNumber::PIN_1;
        case GPIO_PIN_2: return GpioPinNumber::PIN_2;
        case GPIO_PIN_3: return GpioPinNumber::PIN_3;
        case GPIO_PIN_4: return GpioPinNumber::PIN_4;
        case GPIO_PIN_5: return GpioPinNumber::PIN_5;
        case GPIO_PIN_6: return GpioPinNumber::PIN_6;
        case GPIO_PIN_7: return GpioPinNumber::PIN_7;
        case GPIO_PIN_8: return GpioPinNumber::PIN_8;
        case GPIO_PIN_9: return GpioPinNumber::PIN_9;
        case GPIO_PIN_10: return GpioPinNumber::PIN_10;
        case GPIO_PIN_11: return GpioPinNumber::PIN_11;
        case GPIO_PIN_12: return GpioPinNumber::PIN_12;
        case GPIO_PIN_13: return GpioPinNumber::PIN_13;
        case GPIO_PIN_14: return GpioPinNumber::PIN_14;
        case GPIO_PIN_15: return GpioPinNumber::PIN_15;
        default: return GpioPinNumber::PIN_0; // fallback
    }
}

constexpr GpioPortType toGpioPortType(GPIO_TypeDef* hal_port) {
    if (hal_port == GPIOA) return GpioPortType::PORT_A;
    if (hal_port == GPIOB) return GpioPortType::PORT_B;
    if (hal_port == GPIOC) return GpioPortType::PORT_C;
    if (hal_port == GPIOF) return GpioPortType::PORT_F;
#ifdef GPIOD
    if (hal_port == GPIOD) return GpioPortType::PORT_D;
#endif
#ifdef GPIOE
    if (hal_port == GPIOE) return GpioPortType::PORT_E;
#endif
    return GpioPortType::UNKNOWN_PORT; // fallback
}

#endif /* INTERFACE_GPIO_DEFINES_HPP_ */
