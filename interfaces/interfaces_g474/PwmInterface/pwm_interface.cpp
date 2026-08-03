#include "pwm_interface.hpp"

#ifdef HAL_TIM_MODULE_ENABLED

#include "comm_defines.hpp"
#include "hardware_map.hpp"
#include "stm32g4xx_hal.h"

bool PwmInterface::output_started_ = false;

uint32_t PwmInterface::getTimerClockHz() {
  uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
    tim_clk *= 2U;
  }
  return tim_clk;
}

InterfaceStatus PwmInterface::applyPwm(uint32_t frequency_hz, uint16_t duty_percent) {
  CommTimHandle& htim = HardwareMap::pwm_main;

  const uint64_t target_ticks = static_cast<uint64_t>(getTimerClockHz()) / static_cast<uint64_t>(frequency_hz);
  if (target_ticks < 2U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  uint32_t prescaler = 0U;
  uint32_t period = static_cast<uint32_t>(target_ticks - 1U);
  while (period > 0xFFFF0000U) {
    ++prescaler;
    const uint64_t scaled = target_ticks / (static_cast<uint64_t>(prescaler) + 1U);
    if (scaled < 2U) {
      return InterfaceStatus::INTERFACE_ERROR;
    }
    period = static_cast<uint32_t>(scaled - 1U);
  }

  uint32_t pulse = 0U;
  if (duty_percent > 0U) {
    pulse = static_cast<uint32_t>((static_cast<uint64_t>(period + 1U) * duty_percent) / 100U);
    if (pulse > period + 1U) {
      pulse = period + 1U;
    }
  }

  // GPC CubeMX mapping: TIM2 CH4 (PB11).
  static constexpr uint32_t PWM_CHANNEL = TIM_CHANNEL_4;

  __HAL_TIM_SET_PRESCALER(&htim, prescaler);
  __HAL_TIM_SET_AUTORELOAD(&htim, period);
  __HAL_TIM_SET_COMPARE(&htim, PWM_CHANNEL, pulse);
  HAL_TIM_GenerateEvent(&htim, TIM_EVENTSOURCE_UPDATE);

  if (!output_started_) {
    if (HAL_TIM_PWM_Start(&htim, PWM_CHANNEL) != HAL_OK) {
      return InterfaceStatus::INTERFACE_ERROR;
    }
    output_started_ = true;
  }
  return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus PwmInterface::setPwm(uint32_t frequency_hz, uint16_t duty_percent) {
  if (frequency_hz == 0U || duty_percent > 100U) {
    return InterfaceStatus::INTERFACE_ERROR;
  }
  return applyPwm(frequency_hz, duty_percent);
}

#endif  // HAL_TIM_MODULE_ENABLED
