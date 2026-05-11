/*
 * f072_gpio_pins.hpp
 *
 * Common GPIO pin constants for F072 chip configurations
 * Created on: Jan 16, 2025
 *      Author: ariel
 */

#ifndef SRC_CONFIGSTRUCTS_F072_GPIO_PINS_HPP_
#define SRC_CONFIGSTRUCTS_F072_GPIO_PINS_HPP_

#include "gpio_defines.hpp"

// GPIO pin constants for F072 chip configurations
// These are the default pin assignments for F072-based boards
// Individual config files can override these if needed

static constexpr GpioPortType CH_A_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_A_PIN = GpioPinNumber::PIN_1;
static constexpr GpioPortType CH_B_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_B_PIN = GpioPinNumber::PIN_2;
static constexpr GpioPortType CH_C_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_C_PIN = GpioPinNumber::PIN_10;
static constexpr GpioPortType CH_D_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_D_PIN = GpioPinNumber::PIN_12;
static constexpr GpioPortType CH_E_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_E_PIN = GpioPinNumber::PIN_13;
static constexpr GpioPortType CH_F_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_F_PIN = GpioPinNumber::PIN_14;
static constexpr GpioPortType CH_G_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_G_PIN = GpioPinNumber::PIN_15;
static constexpr GpioPortType CH_H_PORT = GpioPortType::PORT_B;
static constexpr GpioPinNumber CH_H_PIN = GpioPinNumber::PIN_15;

#endif /* SRC_CONFIGSTRUCTS_F072_GPIO_PINS_HPP_ */
