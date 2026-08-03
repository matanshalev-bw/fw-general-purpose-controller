#ifndef INTERFACE_PWM_INTERFACE_HPP_
#define INTERFACE_PWM_INTERFACE_HPP_

#include "interface_status.hpp"
#include "stm32g4xx_hal.h"

#ifdef HAL_TIM_MODULE_ENABLED

class PwmInterface {
 public:
  static InterfaceStatus setPwm(uint32_t frequency_hz, uint16_t duty_percent);

 private:
  static bool output_started_;
  static uint32_t getTimerClockHz();
  static InterfaceStatus applyPwm(uint32_t frequency_hz, uint16_t duty_percent);
};

#endif  // HAL_TIM_MODULE_ENABLED

#endif  // INTERFACE_PWM_INTERFACE_HPP_
