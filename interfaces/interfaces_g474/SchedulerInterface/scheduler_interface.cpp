/*
 * timerinterface.cpp
 *
 *  Created on: Jul 10, 2024
 *      Author: matan
 */

#include "scheduler_interface.hpp"

///////////////////////////////// TIMER /////////////////////////////////

SchedulerTimer::SchedulerTimer(TIM_HandleTypeDef *timer, const float freq, bool *timer_flag)
    : timer_(timer), timer_flag_(timer_flag) {
  if (freq) {
    setTimerPeriod(timer, freq);
    HAL_TIM_Base_Start_IT(timer);
  }
}

InterfaceStatus SchedulerTimer::setTimerPeriod(TIM_HandleTypeDef *timer, const float freq) {
  if (not timer or not timer->Instance or freq <= 0.0f) {
        return InterfaceStatus::INTERFACE_ERROR;
  }

  uint32_t periph_tim_freq = HAL_RCC_GetPCLK1Freq();
  const float dev = periph_tim_freq / freq;

  if (dev < 0xFFFF) {
    timer->Init.Prescaler = dev - 1;
    timer->Init.Period = 1;
  } else {
    timer->Init.Prescaler = 0xFFFF - 1;
    timer->Init.Period = dev / 0xFFFF;
  }

  timer->Instance->CNT = 0;  // reset timer counter
  return static_cast<InterfaceStatus>(HAL_TIM_Base_Init(timer));
}

bool SchedulerTimer::isDue() const { return *timer_flag_; }

InterfaceStatus SchedulerTimer::restart() {
  *timer_flag_ = false;
  return static_cast<InterfaceStatus>(HAL_TIM_Base_Start_IT(timer_));
}

//////////////////////////////// TIMER ONCE /////////////////////////////

InterfaceStatus SchedulerTimerOnce::reset(const uint32_t millis) {
  *timer_flag_ = false;
  setTimerPeriod(timer_, 1000.0 / millis);
  return static_cast<InterfaceStatus>(HAL_TIM_Base_Start_IT(timer_));
}

//////////////////////////////// MAIN CLOCK /////////////////////////////

bool SchedulerMainClock::isDue() const { return (HAL_GetTick() - last_time_ > PERIOD_); }

InterfaceStatus SchedulerMainClock::restart() {
  last_time_ = HAL_GetTick();
  return InterfaceStatus::INTERFACE_OK;
}
uint32_t SchedulerMainClock::timeElapsed() const { return HAL_GetTick() - last_time_; }

////////////////////////////// MAIN CLOCK ONCE ///////////////////////////

bool SchedulerOnce::isDue() {
  if (is_finished_)
    return true;
  if (HAL_GetTick() - last_time_ > period_) {
    is_finished_ = true;
    return true;
  }
  return false;
}

InterfaceStatus SchedulerOnce::reset(const uint32_t millis) {
  is_finished_ = (millis == 0);
  period_ = millis;
  last_time_ = HAL_GetTick();
  return InterfaceStatus::INTERFACE_OK;
}

bool SchedulerOnce::isActive() const { return period_ > 0 and not is_finished_; }

uint32_t SchedulerOnce::timeElapsed() const { return HAL_GetTick() - last_time_; }
InterfaceStatus SchedulerOnce::restart() {
  is_finished_ = false;
  last_time_ = HAL_GetTick();
  return InterfaceStatus::INTERFACE_OK;
}
